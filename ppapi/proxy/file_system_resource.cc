// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/file_system_resource.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "ipc/ipc_message.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/file_growth.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_file_io_api.h"

using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_FileIO_API;
using ppapi::thunk::PPB_FileSystem_API;

namespace ppapi {
namespace proxy {

FileSystemResource::QuotaRequest::QuotaRequest(
    int64_t amount_arg,
    const RequestQuotaCallback& callback_arg)
    : amount(amount_arg),
      callback(callback_arg) {
}

FileSystemResource::QuotaRequest::~QuotaRequest() {
}

FileSystemResource::FileSystemResource(Connection connection,
                                       PP_Instance instance,
                                       PP_FileSystemType type)
    : PluginResource(connection, instance),
      type_(type),
      called_open_(false),
      callback_count_(0),
      callback_result_(PP_OK),
      reserved_quota_(0),
      reserving_quota_(false) {
  DCHECK(type_ != PP_FILESYSTEMTYPE_INVALID);
  SendCreate(RENDERER, PpapiHostMsg_FileSystem_Create(type_));
  SendCreate(BROWSER, PpapiHostMsg_FileSystem_Create(type_));
}

FileSystemResource::FileSystemResource(Connection connection,
                                       PP_Instance instance,
                                       int pending_renderer_id,
                                       int pending_browser_id,
                                       PP_FileSystemType type)
    : PluginResource(connection, instance),
      type_(type),
      called_open_(true),
      callback_count_(0),
      callback_result_(PP_OK),
      reserved_quota_(0),
      reserving_quota_(false) {
  DCHECK(type_ != PP_FILESYSTEMTYPE_INVALID);
  AttachToPendingHost(RENDERER, pending_renderer_id);
  AttachToPendingHost(BROWSER, pending_browser_id);
}

FileSystemResource::~FileSystemResource() {
}

PPB_FileSystem_API* FileSystemResource::AsPPB_FileSystem_API() {
  return this;
}

int32_t FileSystemResource::Open(int64_t expected_size,
                                 scoped_refptr<TrackedCallback> callback) {
  DCHECK(type_ != PP_FILESYSTEMTYPE_ISOLATED);
  if (called_open_)
    return PP_ERROR_FAILED;
  called_open_ = true;

  Call<PpapiPluginMsg_FileSystem_OpenReply>(RENDERER,
      PpapiHostMsg_FileSystem_Open(expected_size),
      base::Bind(&FileSystemResource::OpenComplete,
                 this,
                 callback));
  Call<PpapiPluginMsg_FileSystem_OpenReply>(BROWSER,
      PpapiHostMsg_FileSystem_Open(expected_size),
      base::Bind(&FileSystemResource::OpenComplete,
                 this,
                 callback));
  return PP_OK_COMPLETIONPENDING;
}

PP_FileSystemType FileSystemResource::GetType() {
  return type_;
}

void FileSystemResource::OpenQuotaFile(PP_Resource file_io) {
  DCHECK(!base::Contains(files_, file_io));
  files_.insert(file_io);
}

void FileSystemResource::CloseQuotaFile(PP_Resource file_io) {
  DCHECK(base::Contains(files_, file_io));
  files_.erase(file_io);
}

int64_t FileSystemResource::RequestQuota(
    int64_t amount,
    const RequestQuotaCallback& callback) {
  DCHECK(amount >= 0);
  if (!reserving_quota_ && reserved_quota_ >= amount) {
    reserved_quota_ -= amount;
    return amount;
  }

  // Queue up a pending quota request.
  pending_quota_requests_.push(QuotaRequest(amount, callback));

  // Reserve more quota if we haven't already.
  if (!reserving_quota_)
    ReserveQuota(amount);

  return PP_OK_COMPLETIONPENDING;
}

int32_t FileSystemResource::InitIsolatedFileSystem(
    const std::string& fsid,
    PP_IsolatedFileSystemType_Private type,
    const base::Callback<void(int32_t)>& callback) {
  // This call is mutually exclusive with Open() above, so we can reuse the
  // called_open state.
  DCHECK(type_ == PP_FILESYSTEMTYPE_ISOLATED);
  if (called_open_)
    return PP_ERROR_FAILED;
  called_open_ = true;

  Call<PpapiPluginMsg_FileSystem_InitIsolatedFileSystemReply>(RENDERER,
      PpapiHostMsg_FileSystem_InitIsolatedFileSystem(fsid, type),
      base::Bind(&FileSystemResource::InitIsolatedFileSystemComplete,
      this,
      callback));
  Call<PpapiPluginMsg_FileSystem_InitIsolatedFileSystemReply>(BROWSER,
      PpapiHostMsg_FileSystem_InitIsolatedFileSystem(fsid, type),
      base::Bind(&FileSystemResource::InitIsolatedFileSystemComplete,
      this,
      callback));
  return PP_OK_COMPLETIONPENDING;
}

void FileSystemResource::OpenComplete(
    scoped_refptr<TrackedCallback> callback,
    const ResourceMessageReplyParams& params) {
  ++callback_count_;
  // Prioritize worse result since only one status can be returned.
  if (params.result() != PP_OK)
    callback_result_ = params.result();
  // Received callback from browser and renderer.
  if (callback_count_ == 2)
    callback->Run(callback_result_);
}

void FileSystemResource::InitIsolatedFileSystemComplete(
    const base::Callback<void(int32_t)>& callback,
    const ResourceMessageReplyParams& params) {
  ++callback_count_;
  // Prioritize worse result since only one status can be returned.
  if (params.result() != PP_OK)
    callback_result_ = params.result();
  // Received callback from browser and renderer.
  if (callback_count_ == 2)
    callback.Run(callback_result_);
}

void FileSystemResource::ReserveQuota(int64_t amount) {
  DCHECK(!reserving_quota_);
  reserving_quota_ = true;

  FileGrowthMap file_growths;
  for (std::set<PP_Resource>::iterator it = files_.begin();
       it != files_.end(); ++it) {
    EnterResourceNoLock<PPB_FileIO_API> enter(*it, true);
    if (enter.failed()) {
      NOTREACHED();
      continue;
    }
    PPB_FileIO_API* file_io_api = enter.object();
    file_growths[*it] = FileGrowth(
        file_io_api->GetMaxWrittenOffset(),
        file_io_api->GetAppendModeWriteAmount());
  }
  Call<PpapiPluginMsg_FileSystem_ReserveQuotaReply>(BROWSER,
      PpapiHostMsg_FileSystem_ReserveQuota(amount, file_growths),
      base::Bind(&FileSystemResource::ReserveQuotaComplete,
                 this));
}

void FileSystemResource::ReserveQuotaComplete(
    const ResourceMessageReplyParams& params,
    int64_t amount,
    const FileSizeMap& file_sizes) {
  DCHECK(reserving_quota_);
  reserving_quota_ = false;
  reserved_quota_ = amount;

  for (FileSizeMap::const_iterator it = file_sizes.begin();
       it != file_sizes.end(); ++it) {
    EnterResourceNoLock<PPB_FileIO_API> enter(it->first, true);

    // It is possible that the host has sent an offset for a file that has been
    // destroyed in the plugin. Ignore it.
    if (enter.failed())
      continue;
    PPB_FileIO_API* file_io_api = enter.object();
    file_io_api->SetMaxWrittenOffset(it->second);
    file_io_api->SetAppendModeWriteAmount(0);
  }

  DCHECK(!pending_quota_requests_.empty());
  // If we can't grant the first request after refreshing reserved_quota_, then
  // fail all pending quota requests to avoid an infinite refresh/fail loop.
  bool fail_all = reserved_quota_ < pending_quota_requests_.front().amount;
  while (!pending_quota_requests_.empty()) {
    QuotaRequest& request = pending_quota_requests_.front();
    if (fail_all) {
      request.callback.Run(0);
      pending_quota_requests_.pop();
    } else if (reserved_quota_ >= request.amount) {
      reserved_quota_ -= request.amount;
      request.callback.Run(request.amount);
      pending_quota_requests_.pop();
    } else {
      // Refresh the quota reservation for the first pending request that we
      // can't satisfy.
      ReserveQuota(request.amount);
      break;
    }
  }
}

}  // namespace proxy
}  // namespace ppapi
