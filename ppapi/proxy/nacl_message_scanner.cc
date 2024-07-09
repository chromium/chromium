// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/nacl_message_scanner.h"

#include <stddef.h>

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/not_fatal_until.h"
#include "build/build_config.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_message_params.h"
#include "ppapi/proxy/serialized_handle.h"
#include "ppapi/proxy/serialized_var.h"

class NaClDescImcShm;

namespace IPC {
class Message;
}

using ppapi::proxy::ResourceMessageReplyParams;
using ppapi::proxy::SerializedHandle;
using ppapi::proxy::SerializedVar;

namespace {

typedef std::vector<SerializedHandle> Handles;

struct ScanningResults {
  ScanningResults() : handle_index(0), pp_resource(0) {}

  // Vector to hold handles found in the message.
  Handles handles;
  // Current handle index in the rewritten message. During the scan, it will be
  // be less than or equal to handles.size(). After the scan it should be equal.
  int handle_index;
  // The rewritten message. This may be NULL, so all ScanParam overloads should
  // check for NULL before writing to it. In some cases, a ScanParam overload
  // may set this to NULL when it can determine that there are no parameters
  // that need conversion. (See the ResourceMessageReplyParams overload.)
  std::unique_ptr<IPC::Message> new_msg;
  // Resource id for resource messages. Save this when scanning resource replies
  // so when we audit the nested message, we know which resource it is for.
  PP_Resource pp_resource;
  // Callback to receive the nested message in a resource message or reply.
  base::RepeatingCallback<
      void(PP_Resource, const IPC::Message&, SerializedHandle*)>
      nested_msg_callback;
};

void WriteHandle(int handle_index,
                 const SerializedHandle& handle,
                 base::Pickle* msg) {
  SerializedHandle::WriteHeader(handle.header(), msg);

  if (handle.type() == SerializedHandle::SHARED_MEMORY_REGION) {
    // Write the region in POSIX style.
    // This serialization must be kept in sync with
    // ParamTraits<PlatformSharedMemoryRegion>::Write.
    const auto& region = handle.shmem_region();
    if (region.IsValid()) {
      IPC::WriteParam(msg, true);  // valid == true
      IPC::WriteParam(msg, region.GetMode());
      IPC::WriteParam(msg, static_cast<uint64_t>(region.GetSize()));
      IPC::WriteParam(msg, region.GetGUID());
      // Writable regions are not supported, so write only one handle index.
      DCHECK_NE(region.GetMode(),
                base::subtle::PlatformSharedMemoryRegion::Mode::kWritable);
      IPC::WriteParam(msg, handle_index);
    } else {
      msg->WriteBool(false);  // valid == false
    }
  } else if (handle.type() != SerializedHandle::INVALID) {
    // Now write the handle itself in POSIX style.
    // This serialization must be kept in sync with
    // ParamTraits<FileDescriptor>::Write.
    msg->WriteBool(true);  // valid == true
    msg->WriteInt(handle_index);
  }
}

// Define overloads for each kind of message parameter that requires special
// handling. See ScanTuple for how these get used.

// Overload to match SerializedHandle.
void ScanParam(SerializedHandle&& handle, ScanningResults* results) {
  if (results->new_msg)
    WriteHandle(results->handle_index++, handle, results->new_msg.get());
  results->handles.push_back(std::move(handle));
}

void HandleWriter(int* handle_index,
                  base::Pickle* m,
                  const SerializedHandle& handle) {
  WriteHandle((*handle_index)++, handle, m);
}

// Overload to match SerializedVar, which can contain handles.
void ScanParam(SerializedVar&& var, ScanningResults* results) {
  // Rewrite the message and then copy any handles.
  if (results->new_msg)
    var.WriteDataToMessage(
        results->new_msg.get(),
        base::BindRepeating(&HandleWriter, &results->handle_index));
  for (SerializedHandle* var_handle : var.GetHandles())
    results->handles.push_back(std::move(*var_handle));
}

// For PpapiMsg_ResourceReply and the reply to PpapiHostMsg_ResourceSyncCall,
// the handles are carried inside the ResourceMessageReplyParams.
// NOTE: We only intercept handles from host->NaCl. The only kind of
//       ResourceMessageParams that travels this direction is
//       ResourceMessageReplyParams, so that's the only one we need to handle.
void ScanParam(ResourceMessageReplyParams&& params, ScanningResults* results) {
  results->pp_resource = params.pp_resource();
  // If the resource reply params don't contain handles, NULL the new message
  // pointer to cancel further rewriting.
  // NOTE: This works because only handles currently need rewriting, and we
  //       know at this point that this message has none.
  if (params.handles().empty()) {
    results->new_msg.reset(NULL);
    return;
  }

  // If we need to rewrite the message, write everything before the handles
  // (there's nothing after the handles).
  if (results->new_msg) {
    params.WriteReplyHeader(results->new_msg.get());
    // IPC writes the vector length as an int before the contents of the
    // vector.
    results->new_msg->WriteInt(static_cast<int>(params.handles().size()));
  }
  std::vector<SerializedHandle> handles;
  params.TakeAllHandles(&handles);
  for (SerializedHandle& handle : handles) {
    // ScanParam will write each handle to the new message, if necessary.
    ScanParam(std::move(handle), results);
  }
}

// Overload to match nested messages. If we need to rewrite the message, write
// the parameter.
void ScanParam(IPC::Message&& param, ScanningResults* results) {
  if (results->pp_resource && !results->nested_msg_callback.is_null()) {
    SerializedHandle* handle = NULL;
    if (results->handles.size() == 1)
      handle = &results->handles[0];
    results->nested_msg_callback.Run(results->pp_resource, param, handle);
  }
  if (results->new_msg)
    IPC::WriteParam(results->new_msg.get(), param);
}

template <class T>
void ScanParam(std::vector<T>&& vec, ScanningResults* results) {
  if (results->new_msg)
    IPC::WriteParam(results->new_msg.get(), static_cast<int>(vec.size()));
  for (T& element : vec)
    ScanParam(std::move(element), results);
}

// Overload to match all other types. If we need to rewrite the message, write
// the parameter.
template <class T>
void ScanParam(T&& param, ScanningResults* results) {
  if (results->new_msg)
    IPC::WriteParam(results->new_msg.get(), param);
}

// These just break apart the given tuple and run ScanParam over each param.
// The idea is to scan elements in the tuple which require special handling,
// and write them into the |results| struct.
template <class A>
void ScanTuple(std::tuple<A>&& t1, ScanningResults* results) {
  ScanParam(std::move(std::get<0>(t1)), results);
}
template <class A, class B>
void ScanTuple(std::tuple<A, B>&& t1, ScanningResults* results) {
  ScanParam(std::move(std::get<0>(t1)), results);
  ScanParam(std::move(std::get<1>(t1)), results);
}
template <class A, class B, class C>
void ScanTuple(std::tuple<A, B, C>&& t1, ScanningResults* results) {
  ScanParam(std::move(std::get<0>(t1)), results);
  ScanParam(std::move(std::get<1>(t1)), results);
  ScanParam(std::move(std::get<2>(t1)), results);
}
template <class A, class B, class C, class D>
void ScanTuple(std::tuple<A, B, C, D>&& t1, ScanningResults* results) {
  ScanParam(std::move(std::get<0>(t1)), results);
  ScanParam(std::move(std::get<1>(t1)), results);
  ScanParam(std::move(std::get<2>(t1)), results);
  ScanParam(std::move(std::get<3>(t1)), results);
}
template <class A, class B, class C, class D, class E>
void ScanTuple(std::tuple<A, B, C, D, E>&& t1, ScanningResults* results) {
  ScanParam(std::move(std::get<0>(t1)), results);
  ScanParam(std::move(std::get<1>(t1)), results);
  ScanParam(std::move(std::get<2>(t1)), results);
  ScanParam(std::move(std::get<3>(t1)), results);
  ScanParam(std::move(std::get<4>(t1)), results);
}

template <class MessageType>
class MessageScannerImpl {
 public:
  explicit MessageScannerImpl(const IPC::Message* msg) : msg_(msg) {}
  bool ScanMessage(ScanningResults* results) {
    typename MessageType::Param params;
    if (!MessageType::Read(msg_, &params))
      return false;
    ScanTuple(std::move(params), results);
    return true;
  }

  bool ScanSyncMessage(ScanningResults* results) {
    typename MessageType::SendParam params;
    if (!MessageType::ReadSendParam(msg_, &params))
      return false;
    // If we need to rewrite the message, write the message id first.
    if (results->new_msg) {
      results->new_msg->set_sync();
      int id = IPC::SyncMessage::GetMessageId(*msg_);
      results->new_msg->WriteInt(id);
    }
    ScanTuple(std::move(params), results);
    return true;
  }

  bool ScanReply(ScanningResults* results) {
    typename MessageType::ReplyParam params;
    if (!MessageType::ReadReplyParam(msg_, &params))
      return false;
    // If we need to rewrite the message, write the message id first.
    if (results->new_msg) {
      results->new_msg->set_reply();
      int id = IPC::SyncMessage::GetMessageId(*msg_);
      results->new_msg->WriteInt(id);
    }
    ScanTuple(std::move(params), results);
    return true;
  }

 private:
  const IPC::Message* msg_;
};

}  // namespace

#define CASE_FOR_MESSAGE(MESSAGE_TYPE) \
      case MESSAGE_TYPE::ID: { \
        MessageScannerImpl<MESSAGE_TYPE> scanner(&msg); \
        if (rewrite_msg) \
          results.new_msg.reset( \
              new IPC::Message(msg.routing_id(), msg.type(), \
                               IPC::Message::PRIORITY_NORMAL)); \
        if (!scanner.ScanMessage(&results)) \
          return false; \
        break; \
      }
#define CASE_FOR_SYNC_MESSAGE(MESSAGE_TYPE) \
      case MESSAGE_TYPE::ID: { \
        MessageScannerImpl<MESSAGE_TYPE> scanner(&msg); \
        if (rewrite_msg) \
          results.new_msg.reset( \
              new IPC::Message(msg.routing_id(), msg.type(), \
                               IPC::Message::PRIORITY_NORMAL)); \
        if (!scanner.ScanSyncMessage(&results)) \
          return false; \
        break; \
      }
#define CASE_FOR_REPLY(MESSAGE_TYPE) \
      case MESSAGE_TYPE::ID: { \
        MessageScannerImpl<MESSAGE_TYPE> scanner(&msg); \
        if (rewrite_msg) \
          results.new_msg.reset( \
              new IPC::Message(msg.routing_id(), msg.type(), \
                               IPC::Message::PRIORITY_NORMAL)); \
        if (!scanner.ScanReply(&results)) \
          return false; \
        break; \
      }

namespace ppapi {
namespace proxy {

class SerializedHandle;

NaClMessageScanner::FileSystem::FileSystem()
    : reserved_quota_(0) {
}

NaClMessageScanner::FileSystem::~FileSystem() {
}

bool NaClMessageScanner::FileSystem::UpdateReservedQuota(int64_t delta) {
  base::AutoLock lock(lock_);
  if (std::numeric_limits<int64_t>::max() - reserved_quota_ < delta)
    return false;  // reserved_quota_ + delta would overflow.
  if (reserved_quota_ + delta < 0)
    return false;
  reserved_quota_ += delta;
  return true;
}

NaClMessageScanner::FileIO::FileIO(FileSystem* file_system,
                                   int64_t max_written_offset)
    : file_system_(file_system),
      max_written_offset_(max_written_offset) {
}

NaClMessageScanner::FileIO::~FileIO() {
}

void NaClMessageScanner::FileIO::SetMaxWrittenOffset(
    int64_t max_written_offset) {
  base::AutoLock lock(lock_);
  max_written_offset_ = max_written_offset;
}

bool NaClMessageScanner::FileIO::Grow(int64_t amount) {
  base::AutoLock lock(lock_);
  DCHECK(amount > 0);
  if (!file_system_->UpdateReservedQuota(-amount))
    return false;
  max_written_offset_ += amount;
  return true;
}

NaClMessageScanner::NaClMessageScanner() {
}

NaClMessageScanner::~NaClMessageScanner() {
  for (FileSystemMap::iterator it = file_systems_.begin();
      it != file_systems_.end(); ++it)
    delete it->second;
  for (FileIOMap::iterator it = files_.begin(); it != files_.end(); ++it)
    delete it->second;
}

// Windows IPC differs from POSIX in that native handles are serialized in the
// message body, rather than passed in a separate FileDescriptorSet. Therefore,
// on Windows, any message containing handles must be rewritten in the POSIX
// format before we can send it to the NaCl plugin.
// On Mac, base::SharedMemoryHandle has a different serialization than
// base::FileDescriptor (which base::SharedMemoryHandle is typedef-ed to in
// OS_NACL).
bool NaClMessageScanner::ScanMessage(
    const IPC::Message& msg,
    uint32_t type,
    std::vector<SerializedHandle>* handles,
    std::unique_ptr<IPC::Message>* new_msg_ptr) {
  DCHECK(handles);
  DCHECK(handles->empty());
  DCHECK(new_msg_ptr);
  DCHECK(!new_msg_ptr->get());

  bool rewrite_msg =
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
      true;
#else
      false;
#endif

  // We can't always tell from the message ID if rewriting is needed. Therefore,
  // scan any message types that might contain a handle. If we later determine
  // that there are no handles, we can cancel the rewriting by clearing the
  // results.new_msg pointer.
  ScanningResults results;
  results.nested_msg_callback = base::BindRepeating(
      &NaClMessageScanner::AuditNestedMessage, base::Unretained(this));
  switch (type) {
    CASE_FOR_MESSAGE(PpapiMsg_PPBAudio_NotifyAudioStreamCreated)
    CASE_FOR_MESSAGE(PpapiMsg_PPPMessaging_HandleMessage)
    CASE_FOR_MESSAGE(PpapiPluginMsg_ResourceReply)
    CASE_FOR_SYNC_MESSAGE(PpapiMsg_PPPMessageHandler_HandleBlockingMessage)
    CASE_FOR_SYNC_MESSAGE(PpapiMsg_PnaclTranslatorCompileInit)
    CASE_FOR_SYNC_MESSAGE(PpapiMsg_PnaclTranslatorLink)
    CASE_FOR_REPLY(PpapiHostMsg_OpenResource)
    CASE_FOR_REPLY(PpapiHostMsg_PPBGraphics3D_Create)
    CASE_FOR_REPLY(PpapiHostMsg_PPBGraphics3D_CreateTransferBuffer)
    CASE_FOR_REPLY(PpapiHostMsg_PPBImageData_CreateSimple)
    CASE_FOR_REPLY(PpapiHostMsg_ResourceSyncCall)
    CASE_FOR_REPLY(PpapiHostMsg_SharedMemory_CreateSharedMemory)
    default:
      // Do nothing for messages we don't know.
      break;
  }

  // Only messages containing handles need to be rewritten. If no handles are
  // found, don't return the rewritten message either. This must be changed if
  // we ever add new param types that also require rewriting.
  if (!results.handles.empty()) {
    handles->swap(results.handles);
    *new_msg_ptr = std::move(results.new_msg);
  }
  return true;
}

void NaClMessageScanner::ScanUntrustedMessage(
    const IPC::Message& untrusted_msg,
    std::unique_ptr<IPC::Message>* new_msg_ptr) {
  // Audit FileIO and FileSystem messages to ensure that the plugin doesn't
  // exceed its file quota. If we find the message is malformed, just pass it
  // through - we only care about well formed messages to the host.
  if (untrusted_msg.type() == PpapiHostMsg_ResourceCall::ID) {
    ResourceMessageCallParams params;
    IPC::Message nested_msg;
    if (!UnpackMessage<PpapiHostMsg_ResourceCall>(
            untrusted_msg, &params, &nested_msg))
      return;

    switch (nested_msg.type()) {
      case PpapiHostMsg_FileIO_Close::ID: {
        FileIOMap::iterator it = files_.find(params.pp_resource());
        if (it == files_.end())
          return;
        // Audit FileIO Close messages to make sure the plugin reports an
        // accurate file size.
        FileGrowth file_growth;
        if (!UnpackMessage<PpapiHostMsg_FileIO_Close>(
                nested_msg, &file_growth))
          return;

        int64_t trusted_max_written_offset = it->second->max_written_offset();
        delete it->second;
        files_.erase(it);
        // If the plugin is under-reporting, rewrite the message with the
        // trusted value.
        if (trusted_max_written_offset > file_growth.max_written_offset) {
          *new_msg_ptr = std::make_unique<PpapiHostMsg_ResourceCall>(
              params, PpapiHostMsg_FileIO_Close(
                          FileGrowth(trusted_max_written_offset, 0)));
        }
        break;
      }
      case PpapiHostMsg_FileIO_SetLength::ID: {
        FileIOMap::iterator it = files_.find(params.pp_resource());
        if (it == files_.end())
          return;
        // Audit FileIO SetLength messages to make sure the plugin is within
        // the current quota reservation. In addition, deduct the file size
        // increase from the quota reservation.
        int64_t length = 0;
        if (!UnpackMessage<PpapiHostMsg_FileIO_SetLength>(
                nested_msg, &length))
          return;

        // Calculate file size increase, taking care to avoid overflows.
        if (length < 0)
          return;
        int64_t trusted_max_written_offset = it->second->max_written_offset();
        int64_t increase = length - trusted_max_written_offset;
        if (increase <= 0)
          return;
        if (!it->second->Grow(increase)) {
          *new_msg_ptr = std::make_unique<PpapiHostMsg_ResourceCall>(
              params, PpapiHostMsg_FileIO_SetLength(-1));
        }
        break;
      }
      case PpapiHostMsg_FileSystem_ReserveQuota::ID: {
        // Audit FileSystem ReserveQuota messages to make sure the plugin
        // reports accurate file sizes.
        int64_t amount = 0;
        FileGrowthMap file_growths;
        if (!UnpackMessage<PpapiHostMsg_FileSystem_ReserveQuota>(
                nested_msg, &amount, &file_growths))
          return;

        bool audit_failed = false;
        for (FileGrowthMap::iterator it = file_growths.begin();
            it != file_growths.end(); ++it) {
          FileIOMap::iterator file_it = files_.find(it->first);
          if (file_it == files_.end())
            continue;
          int64_t trusted_max_written_offset =
              file_it->second->max_written_offset();
          if (trusted_max_written_offset > it->second.max_written_offset) {
            audit_failed = true;
            it->second.max_written_offset = trusted_max_written_offset;
          }
          if (it->second.append_mode_write_amount < 0) {
            audit_failed = true;
            it->second.append_mode_write_amount = 0;
          }
        }
        if (audit_failed) {
          *new_msg_ptr = std::make_unique<PpapiHostMsg_ResourceCall>(
              params,
              PpapiHostMsg_FileSystem_ReserveQuota(amount, file_growths));
        }
        break;
      }
      case PpapiHostMsg_ResourceDestroyed::ID: {
        // Audit resource destroyed messages to release FileSystems.
        PP_Resource resource;
        if (!UnpackMessage<PpapiHostMsg_ResourceDestroyed>(
                nested_msg, &resource))
          return;
        FileSystemMap::iterator fs_it = file_systems_.find(resource);
        if (fs_it != file_systems_.end()) {
          delete fs_it->second;
          file_systems_.erase(fs_it);
        }
        break;
      }
    }
  }
}

NaClMessageScanner::FileIO* NaClMessageScanner::GetFile(
    PP_Resource file_io) {
  FileIOMap::iterator it = files_.find(file_io);
  CHECK(it != files_.end(), base::NotFatalUntil::M130);
  return it->second;
}

void NaClMessageScanner::AuditNestedMessage(PP_Resource resource,
                                            const IPC::Message& msg,
                                            SerializedHandle* handle) {
  switch (msg.type()) {
    case PpapiPluginMsg_FileIO_OpenReply::ID: {
      // A file that requires quota checking was opened.
      PP_Resource quota_file_system;
      int64_t max_written_offset = 0;
      if (ppapi::UnpackMessage<PpapiPluginMsg_FileIO_OpenReply>(
              msg, &quota_file_system, &max_written_offset)) {
        if (quota_file_system) {
          // Look up the FileSystem by inserting a new one. If it was already
          // present, get the existing one, otherwise construct it.
          FileSystem* file_system = NULL;
          std::pair<FileSystemMap::iterator, bool> insert_result =
              file_systems_.insert(std::make_pair(quota_file_system,
                                                  file_system));
          if (insert_result.second)
            insert_result.first->second = new FileSystem();
          file_system = insert_result.first->second;
          // Create the FileIO.
          DCHECK(files_.find(resource) == files_.end());
          files_.insert(std::make_pair(
              resource,
              new FileIO(file_system, max_written_offset)));
        }
      }
      break;
    }
    case PpapiPluginMsg_FileSystem_ReserveQuotaReply::ID: {
      // The amount of reserved quota for a FileSystem was refreshed.
      int64_t amount = 0;
      FileSizeMap file_sizes;
      if (ppapi::UnpackMessage<PpapiPluginMsg_FileSystem_ReserveQuotaReply>(
          msg, &amount, &file_sizes)) {
        FileSystemMap::iterator it = file_systems_.find(resource);
        CHECK(it != file_systems_.end(), base::NotFatalUntil::M130);
        it->second->UpdateReservedQuota(amount);

        FileSizeMap::const_iterator offset_it = file_sizes.begin();
        for (; offset_it != file_sizes.end(); ++offset_it) {
          FileIOMap::iterator fio_it = files_.find(offset_it->first);
          CHECK(fio_it != files_.end(), base::NotFatalUntil::M130);
          if (fio_it != files_.end())
            fio_it->second->SetMaxWrittenOffset(offset_it->second);
        }
      }
      break;
    }
  }
}

}  // namespace proxy
}  // namespace ppapi
