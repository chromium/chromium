// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/file_io_resource.h"

#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/task_runner_util.h"
#include "ipc/ipc_message.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/array_writer.h"
#include "ppapi/shared_impl/file_ref_create_info.h"
#include "ppapi/shared_impl/file_system_util.h"
#include "ppapi/shared_impl/file_type_conversion.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_file_ref_api.h"
#include "ppapi/thunk/ppb_file_system_api.h"

using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_FileIO_API;
using ppapi::thunk::PPB_FileRef_API;
using ppapi::thunk::PPB_FileSystem_API;

namespace {

// We must allocate a buffer sized according to the request of the plugin. To
// reduce the chance of out-of-memory errors, we cap the read and write size to
// 32MB. This is OK since the API specifies that it may perform a partial read
// or write.
static const int32_t kMaxReadWriteSize = 32 * 1024 * 1024;  // 32MB

// An adapter to let Read() share the same implementation with ReadToArray().
void* DummyGetDataBuffer(void* user_data, uint32_t count, uint32_t size) {
  return user_data;
}

// File thread task to close the file handle.
void DoClose(base::File auto_close_file) {
}

}  // namespace

namespace ppapi {
namespace proxy {

FileIOResource::QueryOp::QueryOp(scoped_refptr<FileHolder> file_holder)
    : file_holder_(file_holder) {
  DCHECK(file_holder_.get());
}

FileIOResource::QueryOp::~QueryOp() {
}

int32_t FileIOResource::QueryOp::DoWork() {
  return file_holder_->file()->GetInfo(&file_info_) ? PP_OK : PP_ERROR_FAILED;
}

FileIOResource::ReadOp::ReadOp(scoped_refptr<FileHolder> file_holder,
                               int64_t offset,
                               int32_t bytes_to_read)
  : file_holder_(file_holder),
    offset_(offset),
    bytes_to_read_(bytes_to_read) {
  DCHECK(file_holder_.get());
}

FileIOResource::ReadOp::~ReadOp() {
}

int32_t FileIOResource::ReadOp::DoWork() {
  DCHECK(!buffer_.get());
  buffer_.reset(new char[bytes_to_read_]);
  return file_holder_->file()->Read(offset_, buffer_.get(), bytes_to_read_);
}

FileIOResource::WriteOp::WriteOp(scoped_refptr<FileHolder> file_holder,
                                 int64_t offset,
                                 std::unique_ptr<char[]> buffer,
                                 int32_t bytes_to_write,
                                 bool append)
    : file_holder_(file_holder),
      offset_(offset),
      buffer_(std::move(buffer)),
      bytes_to_write_(bytes_to_write),
      append_(append) {}

FileIOResource::WriteOp::~WriteOp() {
}

int32_t FileIOResource::WriteOp::DoWork() {
  // In append mode, we can't call Write, since NaCl doesn't implement fcntl,
  // causing the function to call pwrite, which is incorrect.
  if (append_) {
    return file_holder_->file()->WriteAtCurrentPos(buffer_.get(),
                                                   bytes_to_write_);
  } else {
    return file_holder_->file()->Write(offset_, buffer_.get(), bytes_to_write_);
  }
}

FileIOResource::FileIOResource(Connection connection, PP_Instance instance)
    : PluginResource(connection, instance),
      file_system_type_(PP_FILESYSTEMTYPE_INVALID),
      open_flags_(0),
      max_written_offset_(0),
      append_mode_write_amount_(0),
      check_quota_(false),
      called_close_(false) {
  SendCreate(BROWSER, PpapiHostMsg_FileIO_Create());
}

FileIOResource::~FileIOResource() {
  Close();
}

PPB_FileIO_API* FileIOResource::AsPPB_FileIO_API() {
  return this;
}

int32_t FileIOResource::Open(PP_Resource file_ref,
                             int32_t open_flags,
                             scoped_refptr<TrackedCallback> callback) {
  EnterResourceNoLock<PPB_FileRef_API> enter_file_ref(file_ref, true);
  if (enter_file_ref.failed())
    return PP_ERROR_BADRESOURCE;

  PPB_FileRef_API* file_ref_api = enter_file_ref.object();
  const FileRefCreateInfo& create_info = file_ref_api->GetCreateInfo();
  if (!FileSystemTypeIsValid(create_info.file_system_type)) {
    NOTREACHED();
    return PP_ERROR_FAILED;
  }
  int32_t rv = state_manager_.CheckOperationState(
      FileIOStateManager::OPERATION_EXCLUSIVE, false);
  if (rv != PP_OK)
    return rv;

  open_flags_ = open_flags;
  file_system_type_ = create_info.file_system_type;

  if (create_info.file_system_plugin_resource) {
    EnterResourceNoLock<PPB_FileSystem_API> enter_file_system(
        create_info.file_system_plugin_resource, true);
    if (enter_file_system.failed())
      return PP_ERROR_FAILED;
    // Take a reference on the FileSystem resource. The FileIO host uses the
    // FileSystem host for running tasks and checking quota.
    file_system_resource_ = enter_file_system.resource();
  }

  // Take a reference on the FileRef resource while we're opening the file; we
  // don't want the plugin destroying it during the Open operation.
  file_ref_ = enter_file_ref.resource();

  Call<PpapiPluginMsg_FileIO_OpenReply>(BROWSER,
      PpapiHostMsg_FileIO_Open(
          file_ref,
          open_flags),
      base::Bind(&FileIOResource::OnPluginMsgOpenFileComplete, this,
                 callback));

  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_EXCLUSIVE);
  return PP_OK_COMPLETIONPENDING;
}

int32_t FileIOResource::Query(PP_FileInfo* info,
                              scoped_refptr<TrackedCallback> callback) {
  int32_t rv = state_manager_.CheckOperationState(
      FileIOStateManager::OPERATION_EXCLUSIVE, true);
  if (rv != PP_OK)
    return rv;
  if (!info)
    return PP_ERROR_BADARGUMENT;
  if (!FileHolder::IsValid(file_holder_))
    return PP_ERROR_FAILED;

  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_EXCLUSIVE);

  // If the callback is blocking, perform the task on the calling thread.
  if (callback->is_blocking()) {
    int32_t result = PP_ERROR_FAILED;
    base::File::Info file_info;
    // The plugin could release its reference to this instance when we release
    // the proxy lock below.
    scoped_refptr<FileIOResource> protect(this);
    {
      // Release the proxy lock while making a potentially slow file call.
      ProxyAutoUnlock unlock;
      if (file_holder_->file()->GetInfo(&file_info))
        result = PP_OK;
    }
    if (result == PP_OK) {
      // This writes the file info into the plugin's PP_FileInfo struct.
      ppapi::FileInfoToPepperFileInfo(file_info,
                                      file_system_type_,
                                      info);
    }
    state_manager_.SetOperationFinished();
    return result;
  }

  // For the non-blocking case, post a task to the file thread and add a
  // completion task to write the result.
  scoped_refptr<QueryOp> query_op(new QueryOp(file_holder_));
  base::PostTaskAndReplyWithResult(
      PpapiGlobals::Get()->GetFileTaskRunner(),
      FROM_HERE,
      Bind(&FileIOResource::QueryOp::DoWork, query_op),
      RunWhileLocked(Bind(&TrackedCallback::Run, callback)));
  callback->set_completion_task(
      Bind(&FileIOResource::OnQueryComplete, this, query_op, info));

  return PP_OK_COMPLETIONPENDING;
}

int32_t FileIOResource::Touch(PP_Time last_access_time,
                              PP_Time last_modified_time,
                              scoped_refptr<TrackedCallback> callback) {
  int32_t rv = state_manager_.CheckOperationState(
      FileIOStateManager::OPERATION_EXCLUSIVE, true);
  if (rv != PP_OK)
    return rv;

  Call<PpapiPluginMsg_FileIO_GeneralReply>(BROWSER,
      PpapiHostMsg_FileIO_Touch(last_access_time, last_modified_time),
      base::Bind(&FileIOResource::OnPluginMsgGeneralComplete, this,
                 callback));

  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_EXCLUSIVE);
  return PP_OK_COMPLETIONPENDING;
}

int32_t FileIOResource::Read(int64_t offset,
                             char* buffer,
                             int32_t bytes_to_read,
                             scoped_refptr<TrackedCallback> callback) {
  int32_t rv = state_manager_.CheckOperationState(
      FileIOStateManager::OPERATION_READ, true);
  if (rv != PP_OK)
    return rv;

  PP_ArrayOutput output_adapter;
  output_adapter.GetDataBuffer = &DummyGetDataBuffer;
  output_adapter.user_data = buffer;
  return ReadValidated(offset, bytes_to_read, output_adapter, callback);
}

int32_t FileIOResource::ReadToArray(int64_t offset,
                                    int32_t max_read_length,
                                    PP_ArrayOutput* array_output,
                                    scoped_refptr<TrackedCallback> callback) {
  DCHECK(array_output);
  int32_t rv = state_manager_.CheckOperationState(
      FileIOStateManager::OPERATION_READ, true);
  if (rv != PP_OK)
    return rv;

  return ReadValidated(offset, max_read_length, *array_output, callback);
}

int32_t FileIOResource::Write(int64_t offset,
                              const char* buffer,
                              int32_t bytes_to_write,
                              scoped_refptr<TrackedCallback> callback) {
  if (!buffer)
    return PP_ERROR_FAILED;
  if (offset < 0 || bytes_to_write < 0)
    return PP_ERROR_FAILED;
  if (!FileHolder::IsValid(file_holder_))
    return PP_ERROR_FAILED;

  int32_t rv = state_manager_.CheckOperationState(
      FileIOStateManager::OPERATION_WRITE, true);
  if (rv != PP_OK)
    return rv;

  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_WRITE);

  if (check_quota_) {
    int64_t increase = 0;
    uint64_t max_offset = 0;
    bool append = (open_flags_ & PP_FILEOPENFLAG_APPEND) != 0;
    if (append) {
      increase = bytes_to_write;
    } else {
      uint64_t max_offset = offset + bytes_to_write;
      if (max_offset >
          static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
        return PP_ERROR_FAILED;  // amount calculation would overflow.
      }
      increase = static_cast<int64_t>(max_offset) - max_written_offset_;
    }

    if (increase > 0) {
      // Request a quota reservation. This makes the Write asynchronous, so we
      // must copy the plugin's buffer.
      std::unique_ptr<char[]> copy(new char[bytes_to_write]);
      memcpy(copy.get(), buffer, bytes_to_write);
      int64_t result =
          file_system_resource_->AsPPB_FileSystem_API()->RequestQuota(
              increase,
              base::Bind(&FileIOResource::OnRequestWriteQuotaComplete,
                         this,
                         offset,
                         base::Passed(&copy),
                         bytes_to_write,
                         callback));
      if (result == PP_OK_COMPLETIONPENDING)
        return PP_OK_COMPLETIONPENDING;
      DCHECK(result == increase);

      if (append)
        append_mode_write_amount_ += bytes_to_write;
      else
        max_written_offset_ = max_offset;
    }
  }
  return WriteValidated(offset, buffer, bytes_to_write, callback);
}

int32_t FileIOResource::SetLength(int64_t length,
                                  scoped_refptr<TrackedCallback> callback) {
  int32_t rv = state_manager_.CheckOperationState(
      FileIOStateManager::OPERATION_EXCLUSIVE, true);
  if (rv != PP_OK)
    return rv;
  if (length < 0)
    return PP_ERROR_FAILED;

  if (check_quota_) {
    int64_t increase = length - max_written_offset_;
    if (increase > 0) {
      int32_t result =
          file_system_resource_->AsPPB_FileSystem_API()->RequestQuota(
              increase,
              base::Bind(&FileIOResource::OnRequestSetLengthQuotaComplete,
                         this,
                         length, callback));
      if (result == PP_OK_COMPLETIONPENDING) {
        state_manager_.SetPendingOperation(
            FileIOStateManager::OPERATION_EXCLUSIVE);
        return PP_OK_COMPLETIONPENDING;
      }
      DCHECK(result == increase);
      max_written_offset_ = length;
    }
  }

  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_EXCLUSIVE);
  SetLengthValidated(length, callback);
  return PP_OK_COMPLETIONPENDING;
}

int32_t FileIOResource::Flush(scoped_refptr<TrackedCallback> callback) {
  int32_t rv = state_manager_.CheckOperationState(
      FileIOStateManager::OPERATION_EXCLUSIVE, true);
  if (rv != PP_OK)
    return rv;

  Call<PpapiPluginMsg_FileIO_GeneralReply>(BROWSER,
      PpapiHostMsg_FileIO_Flush(),
      base::Bind(&FileIOResource::OnPluginMsgGeneralComplete, this,
                 callback));

  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_EXCLUSIVE);
  return PP_OK_COMPLETIONPENDING;
}

int64_t FileIOResource::GetMaxWrittenOffset() const {
  return max_written_offset_;
}

int64_t FileIOResource::GetAppendModeWriteAmount() const {
  return append_mode_write_amount_;
}

void FileIOResource::SetMaxWrittenOffset(int64_t max_written_offset) {
  max_written_offset_ = max_written_offset;
}

void FileIOResource::SetAppendModeWriteAmount(
    int64_t append_mode_write_amount) {
  append_mode_write_amount_ = append_mode_write_amount;
}

void FileIOResource::Close() {
  if (called_close_)
    return;

  called_close_ = true;
  if (check_quota_) {
    check_quota_ = false;
    file_system_resource_->AsPPB_FileSystem_API()->CloseQuotaFile(
        pp_resource());
  }

  if (file_holder_.get())
    file_holder_.reset();

  Post(BROWSER, PpapiHostMsg_FileIO_Close(
      FileGrowth(max_written_offset_, append_mode_write_amount_)));
}

int32_t FileIOResource::RequestOSFileHandle(
    PP_FileHandle* handle,
    scoped_refptr<TrackedCallback> callback) {
  int32_t rv = state_manager_.CheckOperationState(
      FileIOStateManager::OPERATION_EXCLUSIVE, true);
  if (rv != PP_OK)
    return rv;

  Call<PpapiPluginMsg_FileIO_RequestOSFileHandleReply>(BROWSER,
      PpapiHostMsg_FileIO_RequestOSFileHandle(),
      base::Bind(&FileIOResource::OnPluginMsgRequestOSFileHandleComplete, this,
                 callback, handle));

  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_EXCLUSIVE);
  return PP_OK_COMPLETIONPENDING;
}

FileIOResource::FileHolder::FileHolder(PP_FileHandle file_handle)
    : file_(file_handle) {
}

// static
bool FileIOResource::FileHolder::IsValid(
    const scoped_refptr<FileIOResource::FileHolder>& handle) {
  return handle.get() && handle->file_.IsValid();
}

FileIOResource::FileHolder::~FileHolder() {
  if (file_.IsValid()) {
    base::TaskRunner* file_task_runner =
        PpapiGlobals::Get()->GetFileTaskRunner();
    file_task_runner->PostTask(FROM_HERE,
                               base::BindOnce(&DoClose, Passed(&file_)));
  }
}

int32_t FileIOResource::ReadValidated(int64_t offset,
                                      int32_t bytes_to_read,
                                      const PP_ArrayOutput& array_output,
                                      scoped_refptr<TrackedCallback> callback) {
  if (bytes_to_read < 0)
    return PP_ERROR_FAILED;
  if (!FileHolder::IsValid(file_holder_))
    return PP_ERROR_FAILED;

  state_manager_.SetPendingOperation(FileIOStateManager::OPERATION_READ);

  bytes_to_read = std::min(bytes_to_read, kMaxReadWriteSize);
  if (callback->is_blocking()) {
    char* buffer = static_cast<char*>(
        array_output.GetDataBuffer(array_output.user_data, bytes_to_read, 1));
    int32_t result = PP_ERROR_FAILED;
    // The plugin could release its reference to this instance when we release
    // the proxy lock below.
    scoped_refptr<FileIOResource> protect(this);
    if (buffer) {
      // Release the proxy lock while making a potentially slow file call.
      ProxyAutoUnlock unlock;
      result = file_holder_->file()->Read(offset, buffer, bytes_to_read);
      if (result < 0)
        result = PP_ERROR_FAILED;
    }
    state_manager_.SetOperationFinished();
    return result;
  }

  // For the non-blocking case, post a task to the file thread.
  scoped_refptr<ReadOp> read_op(
      new ReadOp(file_holder_, offset, bytes_to_read));
  base::PostTaskAndReplyWithResult(
      PpapiGlobals::Get()->GetFileTaskRunner(),
      FROM_HERE,
      Bind(&FileIOResource::ReadOp::DoWork, read_op),
      RunWhileLocked(Bind(&TrackedCallback::Run, callback)));
  callback->set_completion_task(
      Bind(&FileIOResource::OnReadComplete, this, read_op, array_output));

  return PP_OK_COMPLETIONPENDING;
}

int32_t FileIOResource::WriteValidated(
    int64_t offset,
    const char* buffer,
    int32_t bytes_to_write,
    scoped_refptr<TrackedCallback> callback) {
  bool append = (open_flags_ & PP_FILEOPENFLAG_APPEND) != 0;
  if (callback->is_blocking()) {
    int32_t result;
    {
      // Release the proxy lock while making a potentially slow file call.
      ProxyAutoUnlock unlock;
      if (append) {
        result = file_holder_->file()->WriteAtCurrentPos(buffer,
                                                         bytes_to_write);
      } else {
        result = file_holder_->file()->Write(offset, buffer, bytes_to_write);
      }
    }
    if (result < 0)
      result = PP_ERROR_FAILED;

    state_manager_.SetOperationFinished();
    return result;
  }

  // For the non-blocking case, post a task to the file thread. We must copy the
  // plugin's buffer at this point.
  std::unique_ptr<char[]> copy(new char[bytes_to_write]);
  memcpy(copy.get(), buffer, bytes_to_write);
  scoped_refptr<WriteOp> write_op(new WriteOp(
      file_holder_, offset, std::move(copy), bytes_to_write, append));
  base::PostTaskAndReplyWithResult(
      PpapiGlobals::Get()->GetFileTaskRunner(),
      FROM_HERE,
      Bind(&FileIOResource::WriteOp::DoWork, write_op),
      RunWhileLocked(Bind(&TrackedCallback::Run, callback)));
  callback->set_completion_task(Bind(&FileIOResource::OnWriteComplete, this));

  return PP_OK_COMPLETIONPENDING;
}

void FileIOResource::SetLengthValidated(
    int64_t length,
    scoped_refptr<TrackedCallback> callback) {
  Call<PpapiPluginMsg_FileIO_GeneralReply>(BROWSER,
      PpapiHostMsg_FileIO_SetLength(length),
      base::Bind(&FileIOResource::OnPluginMsgGeneralComplete, this,
                 callback));

  // On the browser side we grow |max_written_offset_| monotonically, due to the
  // unpredictable ordering of plugin side Write and SetLength calls. Match that
  // behavior here.
  if (max_written_offset_ < length)
    max_written_offset_ = length;
}

int32_t FileIOResource::OnQueryComplete(scoped_refptr<QueryOp> query_op,
                                        PP_FileInfo* info,
                                        int32_t result) {
  DCHECK(state_manager_.get_pending_operation() ==
         FileIOStateManager::OPERATION_EXCLUSIVE);

  if (result == PP_OK) {
    // This writes the file info into the plugin's PP_FileInfo struct.
    ppapi::FileInfoToPepperFileInfo(query_op->file_info(),
                                    file_system_type_,
                                    info);
  }
  state_manager_.SetOperationFinished();
  return result;
}

int32_t FileIOResource::OnReadComplete(scoped_refptr<ReadOp> read_op,
                                       PP_ArrayOutput array_output,
                                       int32_t result) {
  DCHECK(state_manager_.get_pending_operation() ==
         FileIOStateManager::OPERATION_READ);
  if (result >= 0) {
    ArrayWriter output;
    output.set_pp_array_output(array_output);
    if (output.is_valid())
      output.StoreArray(read_op->buffer(), result);
    else
      result = PP_ERROR_FAILED;
  } else {
    // The read operation failed.
    result = PP_ERROR_FAILED;
  }
  state_manager_.SetOperationFinished();
  return result;
}

void FileIOResource::OnRequestWriteQuotaComplete(
    int64_t offset,
    std::unique_ptr<char[]> buffer,
    int32_t bytes_to_write,
    scoped_refptr<TrackedCallback> callback,
    int64_t granted) {
  DCHECK(granted >= 0);
  if (granted == 0) {
    callback->Run(PP_ERROR_NOQUOTA);
    return;
  }
  if (open_flags_ & PP_FILEOPENFLAG_APPEND) {
    DCHECK_LE(bytes_to_write, granted);
    append_mode_write_amount_ += bytes_to_write;
  } else {
    DCHECK_LE(offset + bytes_to_write - max_written_offset_, granted);

    int64_t max_offset = offset + bytes_to_write;
    if (max_written_offset_ < max_offset)
      max_written_offset_ = max_offset;
  }

  if (callback->is_blocking()) {
    int32_t result =
        WriteValidated(offset, buffer.get(), bytes_to_write, callback);
    DCHECK(result != PP_OK_COMPLETIONPENDING);
    callback->Run(result);
  } else {
    bool append = (open_flags_ & PP_FILEOPENFLAG_APPEND) != 0;
    scoped_refptr<WriteOp> write_op(new WriteOp(
        file_holder_, offset, std::move(buffer), bytes_to_write, append));
    base::PostTaskAndReplyWithResult(
        PpapiGlobals::Get()->GetFileTaskRunner(),
        FROM_HERE,
        Bind(&FileIOResource::WriteOp::DoWork, write_op),
        RunWhileLocked(Bind(&TrackedCallback::Run, callback)));
    callback->set_completion_task(Bind(&FileIOResource::OnWriteComplete, this));
  }
}

void FileIOResource::OnRequestSetLengthQuotaComplete(
    int64_t length,
    scoped_refptr<TrackedCallback> callback,
    int64_t granted) {
  DCHECK(granted >= 0);
  if (granted == 0) {
    callback->Run(PP_ERROR_NOQUOTA);
    return;
  }

  DCHECK_LE(length - max_written_offset_, granted);
  if (max_written_offset_ < length)
    max_written_offset_ = length;
  SetLengthValidated(length, callback);
}

int32_t FileIOResource::OnWriteComplete(int32_t result) {
  DCHECK(state_manager_.get_pending_operation() ==
         FileIOStateManager::OPERATION_WRITE);
  // |result| is the return value of WritePlatformFile; -1 indicates failure.
  if (result < 0)
    result = PP_ERROR_FAILED;

  state_manager_.SetOperationFinished();
  return result;
}

void FileIOResource::OnPluginMsgGeneralComplete(
    scoped_refptr<TrackedCallback> callback,
    const ResourceMessageReplyParams& params) {
  DCHECK(state_manager_.get_pending_operation() ==
         FileIOStateManager::OPERATION_EXCLUSIVE ||
         state_manager_.get_pending_operation() ==
         FileIOStateManager::OPERATION_WRITE);
  // End this operation now, so the user's callback can execute another FileIO
  // operation, assuming there are no other pending operations.
  state_manager_.SetOperationFinished();
  callback->Run(params.result());
}

void FileIOResource::OnPluginMsgOpenFileComplete(
    scoped_refptr<TrackedCallback> callback,
    const ResourceMessageReplyParams& params,
    PP_Resource quota_file_system,
    int64_t max_written_offset) {
  DCHECK(state_manager_.get_pending_operation() ==
         FileIOStateManager::OPERATION_EXCLUSIVE);

  // Release the FileRef resource.
  file_ref_.reset();
  int32_t result = params.result();
  if (result == PP_OK) {
    state_manager_.SetOpenSucceed();

    if (quota_file_system) {
      DCHECK(quota_file_system == file_system_resource_->pp_resource());
      check_quota_ = true;
      max_written_offset_ = max_written_offset;
      file_system_resource_->AsPPB_FileSystem_API()->OpenQuotaFile(
          pp_resource());
    }

    IPC::PlatformFileForTransit transit_file;
    if (params.TakeFileHandleAtIndex(0, &transit_file)) {
      file_holder_ = new FileHolder(
          IPC::PlatformFileForTransitToPlatformFile(transit_file));
    }
  }
  // End this operation now, so the user's callback can execute another FileIO
  // operation, assuming there are no other pending operations.
  state_manager_.SetOperationFinished();
  callback->Run(result);
}

void FileIOResource::OnPluginMsgRequestOSFileHandleComplete(
    scoped_refptr<TrackedCallback> callback,
    PP_FileHandle* output_handle,
    const ResourceMessageReplyParams& params) {
  DCHECK(state_manager_.get_pending_operation() ==
         FileIOStateManager::OPERATION_EXCLUSIVE);

  if (!TrackedCallback::IsPending(callback)) {
    state_manager_.SetOperationFinished();
    return;
  }

  int32_t result = params.result();
  IPC::PlatformFileForTransit transit_file;
  if (!params.TakeFileHandleAtIndex(0, &transit_file))
    result = PP_ERROR_FAILED;
  *output_handle = IPC::PlatformFileForTransitToPlatformFile(transit_file);

  // End this operation now, so the user's callback can execute another FileIO
  // operation, assuming there are no other pending operations.
  state_manager_.SetOperationFinished();
  callback->Run(result);
}

}  // namespace proxy
}  // namespace ppapi
