// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_URL_LOADER_HOST_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_URL_LOADER_HOST_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/resource_message_params.h"
#include "ppapi/shared_impl/url_request_info_data.h"
#include "ppapi/shared_impl/url_response_info_data.h"
#include "third_party/blink/public/web/web_associated_url_loader_client.h"

namespace blink {
class WebAssociatedURLLoader;
class WebLocalFrame;
}  // namespace blink

namespace content {

class RendererPpapiHostImpl;

class PepperURLLoaderHost : public ppapi::host::ResourceHost,
                            public blink::WebAssociatedURLLoaderClient {
 public:
  // If main_document_loader is true, PP_Resource must be 0 since it will be
  // pending until the plugin resource attaches to it.
  PepperURLLoaderHost(RendererPpapiHostImpl* host,
                      bool main_document_loader,
                      PP_Instance instance,
                      PP_Resource resource);
  ~PepperURLLoaderHost() override;

  // ResourceHost implementation.
  int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) override;

  // blink::WebAssociatedURLLoaderClient implementation.
  bool WillFollowRedirect(const blink::WebURL& new_url,
                          const blink::WebURLResponse& redir_response) override;
  void DidSendData(uint64_t bytes_sent,
                   uint64_t total_bytes_to_be_sent) override;
  void DidReceiveResponse(const blink::WebURLResponse& response) override;
  void DidDownloadData(uint64_t data_length) override;
  void DidReceiveData(const char* data, int data_length) override;
  void DidFinishLoading() override;
  void DidFail(const blink::WebURLError& error) override;

 private:
  // ResourceHost protected overrides.
  void DidConnectPendingHostToResource() override;

  // IPC messages
  int32_t OnHostMsgOpen(ppapi::host::HostMessageContext* context,
                        const ppapi::URLRequestInfoData& request_data);
  int32_t InternalOnHostMsgOpen(ppapi::host::HostMessageContext* context,
                                const ppapi::URLRequestInfoData& request_data);
  int32_t OnHostMsgSetDeferLoading(ppapi::host::HostMessageContext* context,
                                   bool defers_loading);
  int32_t OnHostMsgClose(ppapi::host::HostMessageContext* context);
  int32_t OnHostMsgGrantUniversalAccess(
      ppapi::host::HostMessageContext* context);

  // Sends or queues an unsolicited message to the plugin resource. This
  // handles cases where messages must be reordered for the plugin and
  // the case where we have created a pending host resource and the
  // plugin has not connected to us yet.
  //
  // Takes ownership of the given pointer.
  void SendUpdateToPlugin(std::unique_ptr<IPC::Message> msg);

  // Sends or queues an unsolicited message to the plugin resource. This is
  // used inside SendUpdateToPlugin for messages that are already ordered
  // properly.
  //
  // Takes ownership of the given pointer.
  void SendOrderedUpdateToPlugin(std::unique_ptr<IPC::Message> msg);

  void Close();

  // Returns the frame for the current request.
  blink::WebLocalFrame* GetFrame();

  // Calls SetDefersLoading on the current load. This encapsulates the logic
  // differences between document loads and regular ones.
  void SetDefersLoading(bool defers_loading);

  // Converts a WebURLResponse to a URLResponseInfo and saves it.
  void SaveResponse(const blink::WebURLResponse& response);

  // Sends the UpdateProgress message (if necessary) to the plugin.
  void UpdateProgress();

  // Non-owning pointer.
  RendererPpapiHostImpl* renderer_ppapi_host_;

  // If true, then the plugin instance is a full-frame plugin and we're just
  // wrapping the main document's loader (i.e. loader_ is null).
  bool main_document_loader_;

  // The data that generated the request.
  ppapi::URLRequestInfoData request_data_;

  // Set to true when this loader can ignore same originl policy.
  bool has_universal_access_;

  // The loader associated with this request. MAY BE NULL.
  //
  // This will be NULL if the load hasn't been opened yet, or if this is a main
  // document loader (when registered as a mime type). Therefore, you should
  // always NULL check this value before using it. In the case of a main
  // document load, you would call the functions on the document to cancel the
  // load, etc. since there is no loader.
  std::unique_ptr<blink::WebAssociatedURLLoader> loader_;

  int64_t bytes_sent_;
  int64_t total_bytes_to_be_sent_;
  int64_t bytes_received_;
  int64_t total_bytes_to_be_received_;

  // Messages sent while the resource host is pending. These will be forwarded
  // to the plugin when the plugin side connects. The pointers are owned by
  // this object and must be deleted.
  std::vector<std::unique_ptr<IPC::Message>> pending_replies_;
  std::vector<std::unique_ptr<IPC::Message>> out_of_order_replies_;

  // True when there's a pending DataFromURLResponse call which will send a
  // PpapiPluginMsg_URLLoader_ReceivedResponse to the plugin, which introduces
  // ordering constraints on following messages to the plugin.
  bool pending_response_;

  DISALLOW_COPY_AND_ASSIGN(PepperURLLoaderHost);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_URL_LOADER_HOST_H_
