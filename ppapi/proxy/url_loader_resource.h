// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_URL_LOADER_RESOURCE_H_
#define PPAPI_PROXY_URL_LOADER_RESOURCE_H_

#include <stddef.h>
#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/containers/circular_deque.h"
#include "ppapi/c/trusted/ppb_url_loader_trusted.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/url_request_info_data.h"
#include "ppapi/thunk/ppb_url_loader_api.h"

namespace ppapi {

struct URLResponseInfoData;

namespace proxy {

class URLResponseInfoResource;

class PPAPI_PROXY_EXPORT URLLoaderResource : public PluginResource,
                                             public thunk::PPB_URLLoader_API {
 public:
  // Constructor for plugin-initiated loads.
  URLLoaderResource(Connection connection,
                    PP_Instance instance);

  // Constructor for renderer-initiated (document) loads. The loader ID is the
  // pending host ID for the already-created host in the renderer, and the
  // response data is the response for the already-opened connection.
  URLLoaderResource(Connection connection,
                    PP_Instance instance,
                    int pending_main_document_loader_id,
                    const URLResponseInfoData& data);

  URLLoaderResource(const URLLoaderResource&) = delete;
  URLLoaderResource& operator=(const URLLoaderResource&) = delete;

  ~URLLoaderResource() override;

  // Resource override.
  thunk::PPB_URLLoader_API* AsPPB_URLLoader_API() override;

  // PPB_URLLoader_API implementation.
  int32_t Open(PP_Resource request_id,
               scoped_refptr<TrackedCallback> callback) override;
  int32_t Open(const URLRequestInfoData& data,
               int requestor_pid,
               scoped_refptr<TrackedCallback> callback) override;
  int32_t FollowRedirect(scoped_refptr<TrackedCallback> callback) override;
  PP_Bool GetUploadProgress(int64_t* bytes_sent,
                            int64_t* total_bytes_to_be_sent) override;
  PP_Bool GetDownloadProgress(
      int64_t* bytes_received,
      int64_t* total_bytes_to_be_received) override;
  PP_Resource GetResponseInfo() override;
  int32_t ReadResponseBody(
      void* buffer,
      int32_t bytes_to_read,
      scoped_refptr<TrackedCallback> callback) override;
  int32_t FinishStreamingToFile(
      scoped_refptr<TrackedCallback> callback) override;
  void Close() override;
  void GrantUniversalAccess() override;
  void RegisterStatusCallback(
      PP_URLLoaderTrusted_StatusCallback callback) override;

  // PluginResource implementation.
  void OnReplyReceived(const ResourceMessageReplyParams& params,
                       const IPC::Message& msg) override;

 private:
  enum Mode {
    // The plugin has not called Open() yet.
    MODE_WAITING_TO_OPEN,

    // The plugin is waiting for the Open() or FollowRedirect callback.
    MODE_OPENING,

    // We've started to receive data and may receive more.
    MODE_STREAMING_DATA,

    // All data has been streamed or there was an error.
    MODE_LOAD_COMPLETE
  };

  // IPC message handlers.
  void OnPluginMsgReceivedResponse(const ResourceMessageReplyParams& params,
                                   const URLResponseInfoData& data);
  void OnPluginMsgSendData(const ResourceMessageReplyParams& params,
                           const IPC::Message& message);
  void OnPluginMsgFinishedLoading(const ResourceMessageReplyParams& params,
                                  int32_t result);
  void OnPluginMsgUpdateProgress(const ResourceMessageReplyParams& params,
                                 int64_t bytes_sent,
                                 int64_t total_bytes_to_be_sent,
                                 int64_t bytes_received,
                                 int64_t total_bytes_to_be_received);

  // Sends the defers loading message to the renderer to block or unblock the
  // load.
  void SetDefersLoading(bool defers_loading);

  int32_t ValidateCallback(scoped_refptr<TrackedCallback> callback);

  // Sets up |callback| as the pending callback. This should only be called once
  // it is certain that |PP_OK_COMPLETIONPENDING| will be returned.
  void RegisterCallback(scoped_refptr<TrackedCallback> callback);

  void RunCallback(int32_t result);

  // Saves the given response info to response_info_, handling file refs if
  // necessary. This does not issue any callbacks.
  void SaveResponseInfo(const URLResponseInfoData& data);

  int32_t FillUserBuffer();

  Mode mode_;
  URLRequestInfoData request_data_;

  scoped_refptr<TrackedCallback> pending_callback_;

  PP_URLLoaderTrusted_StatusCallback status_callback_;

  base::circular_deque<char> buffer_;
  int64_t bytes_sent_;
  int64_t total_bytes_to_be_sent_;
  int64_t bytes_received_;
  int64_t total_bytes_to_be_received_;
  char* user_buffer_;
  size_t user_buffer_size_;
  int32_t done_status_;
  bool is_streaming_to_file_;
  bool is_asynchronous_load_suspended_;

  // The response info if we've received it.
  scoped_refptr<URLResponseInfoResource> response_info_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_URL_LOADER_RESOURCE_H_
