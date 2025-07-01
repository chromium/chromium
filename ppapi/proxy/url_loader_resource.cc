// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/proxy/url_loader_resource.h"

#include <algorithm>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_url_loader.h"
#include "ppapi/proxy/dispatch_reply_message.h"
#include "ppapi/proxy/file_ref_resource.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/url_request_info_resource.h"
#include "ppapi/proxy/url_response_info_resource.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/url_response_info_data.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/resource_creation_api.h"

using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_URLLoader_API;
using ppapi::thunk::PPB_URLRequestInfo_API;

namespace ppapi {
namespace proxy {

URLLoaderResource::URLLoaderResource(Connection connection,
                                     PP_Instance instance)
    : PluginResource(connection, instance),
      mode_(MODE_WAITING_TO_OPEN),
      status_callback_(NULL),
      bytes_sent_(0),
      total_bytes_to_be_sent_(-1),
      bytes_received_(0),
      total_bytes_to_be_received_(-1),
      user_buffer_(NULL),
      user_buffer_size_(0),
      done_status_(PP_OK_COMPLETIONPENDING),
      is_streaming_to_file_(false),
      is_asynchronous_load_suspended_(false) {
  SendCreate(RENDERER, PpapiHostMsg_URLLoader_Create());
}

URLLoaderResource::URLLoaderResource(Connection connection,
                                     PP_Instance instance,
                                     int pending_main_document_loader_id,
                                     const ppapi::URLResponseInfoData& data)
    : PluginResource(connection, instance),
      mode_(MODE_OPENING),
      status_callback_(NULL),
      bytes_sent_(0),
      total_bytes_to_be_sent_(-1),
      bytes_received_(0),
      total_bytes_to_be_received_(-1),
      user_buffer_(NULL),
      user_buffer_size_(0),
      done_status_(PP_OK_COMPLETIONPENDING),
      is_streaming_to_file_(false),
      is_asynchronous_load_suspended_(false) {
  AttachToPendingHost(RENDERER, pending_main_document_loader_id);
  SaveResponseInfo(data);
}

URLLoaderResource::~URLLoaderResource() {
}

PPB_URLLoader_API* URLLoaderResource::AsPPB_URLLoader_API() {
  return this;
}

int32_t URLLoaderResource::Open(PP_Resource request_id,
                                scoped_refptr<TrackedCallback> callback) {
  EnterResourceNoLock<PPB_URLRequestInfo_API> enter_request(request_id, true);
  if (enter_request.failed()) {
    Log(PP_LOGLEVEL_ERROR,
        "PPB_URLLoader.Open: invalid request resource ID. (Hint to C++ wrapper"
        " users: use the ResourceRequest constructor that takes an instance or"
        " else the request will be null.)");
    return PP_ERROR_BADARGUMENT;
  }
  return Open(enter_request.object()->GetData(), 0, callback);
}

int32_t URLLoaderResource::Open(
    const ::ppapi::URLRequestInfoData& request_data,
    int requestor_pid,
    scoped_refptr<TrackedCallback> callback) {
  int32_t rv = ValidateCallback(callback);
  if (rv != PP_OK)
    return rv;
  if (mode_ != MODE_WAITING_TO_OPEN)
    return PP_ERROR_INPROGRESS;

  request_data_ = request_data;

  mode_ = MODE_OPENING;
  is_asynchronous_load_suspended_ = false;

  RegisterCallback(callback);
  Post(RENDERER, PpapiHostMsg_URLLoader_Open(request_data));
  return PP_OK_COMPLETIONPENDING;
}

int32_t URLLoaderResource::FollowRedirect(
    scoped_refptr<TrackedCallback> callback) {
  int32_t rv = ValidateCallback(callback);
  if (rv != PP_OK)
    return rv;
  if (mode_ != MODE_OPENING)
    return PP_ERROR_INPROGRESS;

  SetDefersLoading(false);  // Allow the redirect to continue.
  RegisterCallback(callback);
  return PP_OK_COMPLETIONPENDING;
}

PP_Bool URLLoaderResource::GetUploadProgress(int64_t* bytes_sent,
                                              int64_t* total_bytes_to_be_sent) {
  if (!request_data_.record_upload_progress) {
    *bytes_sent = 0;
    *total_bytes_to_be_sent = 0;
    return PP_FALSE;
  }
  *bytes_sent = bytes_sent_;
  *total_bytes_to_be_sent = total_bytes_to_be_sent_;
  return PP_TRUE;
}

PP_Bool URLLoaderResource::GetDownloadProgress(
    int64_t* bytes_received,
    int64_t* total_bytes_to_be_received) {
  if (!request_data_.record_download_progress) {
    *bytes_received = 0;
    *total_bytes_to_be_received = 0;
    return PP_FALSE;
  }
  *bytes_received = bytes_received_;
  *total_bytes_to_be_received = total_bytes_to_be_received_;
  return PP_TRUE;
}

PP_Resource URLLoaderResource::GetResponseInfo() {
  if (response_info_.get())
    return response_info_->GetReference();
  return 0;
}

int32_t URLLoaderResource::ReadResponseBody(
    void* buffer,
    int32_t bytes_to_read,
    scoped_refptr<TrackedCallback> callback) {
  int32_t rv = ValidateCallback(callback);
  if (rv != PP_OK)
    return rv;
  if (!response_info_.get())
    return PP_ERROR_FAILED;

  if (bytes_to_read <= 0 || !buffer)
    return PP_ERROR_BADARGUMENT;

  user_buffer_ = static_cast<char*>(buffer);
  user_buffer_size_ = bytes_to_read;

  if (!buffer_.empty())
    return FillUserBuffer();

  // We may have already reached EOF.
  if (done_status_ != PP_OK_COMPLETIONPENDING) {
    user_buffer_ = NULL;
    user_buffer_size_ = 0;
    return done_status_;
  }

  RegisterCallback(callback);
  return PP_OK_COMPLETIONPENDING;
}

int32_t URLLoaderResource::FinishStreamingToFile(
    scoped_refptr<TrackedCallback> callback) {
  return PP_ERROR_NOTSUPPORTED;
}

void URLLoaderResource::Close() {
  mode_ = MODE_LOAD_COMPLETE;
  done_status_ = PP_ERROR_ABORTED;

  Post(RENDERER, PpapiHostMsg_URLLoader_Close());

  // Abort the callbacks, the plugin doesn't want to be called back after this.
  if (TrackedCallback::IsPending(pending_callback_))
    pending_callback_->PostAbort();
}

void URLLoaderResource::GrantUniversalAccess() {
  Post(RENDERER, PpapiHostMsg_URLLoader_GrantUniversalAccess());
}

void URLLoaderResource::RegisterStatusCallback(
    PP_URLLoaderTrusted_StatusCallback callback) {
  status_callback_ = callback;
}

void URLLoaderResource::OnReplyReceived(
    const ResourceMessageReplyParams& params,
    const IPC::Message& msg) {
  PPAPI_BEGIN_MESSAGE_MAP(URLLoaderResource, msg)
    case PpapiPluginMsg_URLLoader_SendData::ID:
      // Special message, manually dispatch since we don't want the automatic
      // unpickling.
      OnPluginMsgSendData(params, msg);
      break;

    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL(
        PpapiPluginMsg_URLLoader_ReceivedResponse,
        OnPluginMsgReceivedResponse)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL(
        PpapiPluginMsg_URLLoader_FinishedLoading,
        OnPluginMsgFinishedLoading)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL(
        PpapiPluginMsg_URLLoader_UpdateProgress,
        OnPluginMsgUpdateProgress)
  PPAPI_END_MESSAGE_MAP()
}

void URLLoaderResource::OnPluginMsgReceivedResponse(
    const ResourceMessageReplyParams& params,
    const URLResponseInfoData& data) {
  SaveResponseInfo(data);
  RunCallback(PP_OK);
}

void URLLoaderResource::OnPluginMsgSendData(
    const ResourceMessageReplyParams& params,
    const IPC::Message& message) {
  base::PickleIterator iter(message);
  const char* data;
  size_t data_length;
  if (!iter.ReadData(&data, &data_length)) {
    NOTREACHED() << "Expecting data";
  }

  mode_ = MODE_STREAMING_DATA;
  buffer_.insert(buffer_.end(), data, data + data_length);

  // To avoid letting the network stack download an entire stream all at once,
  // defer loading when we have enough buffer.
  // Check for this before we run the callback, even though that could move
  // data out of the buffer. Doing anything after the callback is unsafe.
  DCHECK(request_data_.prefetch_buffer_lower_threshold <
         request_data_.prefetch_buffer_upper_threshold);
  if (!is_streaming_to_file_ &&
      !is_asynchronous_load_suspended_ &&
      (buffer_.size() >= static_cast<size_t>(
          request_data_.prefetch_buffer_upper_threshold))) {
    DVLOG(1) << "Suspending async load - buffer size: " << buffer_.size();
    SetDefersLoading(true);
  }

  if (user_buffer_)
    RunCallback(FillUserBuffer());
  else
    DCHECK(!TrackedCallback::IsPending(pending_callback_));
}

void URLLoaderResource::OnPluginMsgFinishedLoading(
    const ResourceMessageReplyParams& params,
    int32_t result) {
  mode_ = MODE_LOAD_COMPLETE;
  done_status_ = result;
  user_buffer_ = NULL;
  user_buffer_size_ = 0;

  // If the client hasn't called any function that takes a callback since
  // the initial call to Open, or called ReadResponseBody and got a
  // synchronous return, then the callback will be NULL.
  if (TrackedCallback::IsPending(pending_callback_))
    RunCallback(done_status_);
}

void URLLoaderResource::OnPluginMsgUpdateProgress(
    const ResourceMessageReplyParams& params,
    int64_t bytes_sent,
    int64_t total_bytes_to_be_sent,
    int64_t bytes_received,
    int64_t total_bytes_to_be_received) {
  bytes_sent_ = bytes_sent;
  total_bytes_to_be_sent_ = total_bytes_to_be_sent;
  bytes_received_ = bytes_received;
  total_bytes_to_be_received_ = total_bytes_to_be_received;

  if (status_callback_) {
    status_callback_(pp_instance(), pp_resource(),
                     bytes_sent_, total_bytes_to_be_sent_,
                     bytes_received_, total_bytes_to_be_received_);
  }
}

void URLLoaderResource::SetDefersLoading(bool defers_loading) {
  is_asynchronous_load_suspended_ = defers_loading;
  Post(RENDERER, PpapiHostMsg_URLLoader_SetDeferLoading(defers_loading));
}

int32_t URLLoaderResource::ValidateCallback(
    scoped_refptr<TrackedCallback> callback) {
  DCHECK(callback.get());
  if (TrackedCallback::IsPending(pending_callback_))
    return PP_ERROR_INPROGRESS;
  return PP_OK;
}

void URLLoaderResource::RegisterCallback(
    scoped_refptr<TrackedCallback> callback) {
  DCHECK(!TrackedCallback::IsPending(pending_callback_));
  pending_callback_ = callback;
}

void URLLoaderResource::RunCallback(int32_t result) {
  // This may be null when this is a main document loader.
  if (!pending_callback_.get())
    return;

  // If |user_buffer_| was set as part of registering a callback, the paths
  // which trigger that callack must have cleared it since the callback is now
  // free to delete it.
  DCHECK(!user_buffer_);

  // As a second line of defense, clear the |user_buffer_| in case the
  // callbacks get called in an unexpected order.
  user_buffer_ = NULL;
  user_buffer_size_ = 0;
  pending_callback_->Run(result);
}

void URLLoaderResource::SaveResponseInfo(const URLResponseInfoData& data) {
  response_info_ =
      new URLResponseInfoResource(connection(), pp_instance(), data);
}

int32_t URLLoaderResource::FillUserBuffer() {
  DCHECK(user_buffer_);
  DCHECK(user_buffer_size_);

  size_t bytes_to_copy = std::min(buffer_.size(), user_buffer_size_);
  std::copy(buffer_.begin(), buffer_.begin() + bytes_to_copy, user_buffer_);
  buffer_.erase(buffer_.begin(), buffer_.begin() + bytes_to_copy);

  // If the buffer is getting too empty, resume asynchronous loading.
  if (is_asynchronous_load_suspended_ &&
      buffer_.size() <= static_cast<size_t>(
          request_data_.prefetch_buffer_lower_threshold)) {
    DVLOG(1) << "Resuming async load - buffer size: " << buffer_.size();
    SetDefersLoading(false);
  }

  // Reset for next time.
  user_buffer_ = NULL;
  user_buffer_size_ = 0;
  return base::checked_cast<int32_t>(bytes_to_copy);
}

}  // namespace proxy
}  // namespace ppapi
