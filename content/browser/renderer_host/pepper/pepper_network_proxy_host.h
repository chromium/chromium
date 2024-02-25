// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_NETWORK_PROXY_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_NETWORK_PROXY_HOST_H_

#include <stdint.h>

#include <optional>
#include <queue>
#include <set>
#include <string>

#include "base/containers/queue.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/weak_ptr.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/resource_host.h"
#include "url/gurl.h"

namespace net {
class ProxyInfo;
}

namespace ppapi {
namespace host {
struct ReplyMessageContext;
}
}

namespace content {

class BrowserPpapiHostImpl;
class PepperProxyLookupHelper;

// The host for PPB_NetworkProxy. This class lives on the IO thread.
class PepperNetworkProxyHost : public ppapi::host::ResourceHost {
 public:
  PepperNetworkProxyHost(BrowserPpapiHostImpl* host,
                         PP_Instance instance,
                         PP_Resource resource);

  PepperNetworkProxyHost(const PepperNetworkProxyHost&) = delete;
  PepperNetworkProxyHost& operator=(const PepperNetworkProxyHost&) = delete;

  ~PepperNetworkProxyHost() override;

 private:
  // We retrieve whether this API is allowed for the instance on the UI thread
  // and pass it to DidGetUIThreadData, which sets allowed_.
  struct UIThreadData {
    UIThreadData();
    UIThreadData(const UIThreadData& other);
    ~UIThreadData();

    bool is_allowed;
  };
  static UIThreadData GetUIThreadDataOnUIThread(int render_process_id,
                                                int render_frame_id,
                                                bool is_external_plugin);
  void DidGetUIThreadData(const UIThreadData&);

  // ResourceHost implementation.
  int32_t OnResourceMessageReceived(
      const IPC::Message& msg,
      ppapi::host::HostMessageContext* context) override;

  int32_t OnMsgGetProxyForURL(ppapi::host::HostMessageContext* context,
                              const std::string& url);

  // Send all messages in |unsent_requests_|.
  void TryToSendUnsentRequests();

  void OnResolveProxyCompleted(ppapi::host::ReplyMessageContext context,
                               PepperProxyLookupHelper* pending_request,
                               std::optional<net::ProxyInfo> proxy_info);
  void SendFailureReply(int32_t error,
                        ppapi::host::ReplyMessageContext context);

  // Used to find correct NetworkContext to perform proxy lookups.
  int render_process_id_;
  int render_frame_id_;

  // The following member is invalid until we get some information from the UI
  // thread. However, it is only ever set or accessed on the IO thread.
  bool is_allowed_;

  // True initially, but set to false once is_allowed_ has been set.
  bool waiting_for_ui_thread_data_;

  // We have to get is_allowed_ from the UI thread before we can start a
  // request. If we receive any calls for GetProxyForURL before is_allowed_ is
  // available, we save them in unsent_requests_.
  struct UnsentRequest {
    GURL url;
    ppapi::host::ReplyMessageContext reply_context;
  };
  base::queue<UnsentRequest> unsent_requests_;

  // Requests awaiting a response from the network service. We need to store
  // these so that we can cancel them if we get destroyed.
  std::set<std::unique_ptr<PepperProxyLookupHelper>, base::UniquePtrComparator>
      pending_requests_;

  base::WeakPtrFactory<PepperNetworkProxyHost> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_NETWORK_PROXY_HOST_H_
