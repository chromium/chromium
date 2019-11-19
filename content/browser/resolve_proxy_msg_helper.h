// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RESOLVE_PROXY_MSG_HELPER_H_
#define CONTENT_BROWSER_RESOLVE_PROXY_MSG_HELPER_H_

#include <string>

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/sequenced_task_runner_helpers.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_message_filter.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/mojom/proxy_lookup_client.mojom.h"
#include "url/gurl.h"

namespace net {
class ProxyInfo;
}

namespace content {

// Responds to ChildProcessHostMsg_ResolveProxy, kicking off a proxy lookup
// request on the UI thread using the specified proxy service.  Completion is
// notified through the delegate.  If multiple requests are started at the same
// time, they will run in FIFO order, with only 1 being outstanding at a time.
//
// When an instance of ResolveProxyMsgHelper is destroyed, it cancels any
// outstanding proxy resolve requests with the proxy service. It also deletes
// the stored IPC::Message pointers for pending requests.
//
// This object does most of its work on the UI thread. It holds onto a
// self-reference as long as there's a pending Mojo call, as losing its last
// reference on the IO thread with an open mojo pipe that lives on the UI
// thread leads to problems.
class CONTENT_EXPORT ResolveProxyMsgHelper : public BrowserMessageFilter,
                                             network::mojom::ProxyLookupClient {
 public:
  explicit ResolveProxyMsgHelper(int render_process_host_id);

  // BrowserMessageFilter implementation
  void OverrideThreadForMessage(const IPC::Message& message,
                                BrowserThread::ID* thread) override;
  bool OnMessageReceived(const IPC::Message& message) override;

  void OnResolveProxy(const GURL& url, IPC::Message* reply_msg);

 protected:
  // Destruction cancels the current outstanding request, and clears the
  // pending queue.
  ~ResolveProxyMsgHelper() override;

 private:
  // Used to destroy the |ResolveProxyMsgHelper| on the UI thread.
  friend class base::DeleteHelper<ResolveProxyMsgHelper>;

  // Starts the first pending request.
  void StartPendingRequest();

  // Virtual for testing. Returns false if unable to get a network service, due
  // to the RenderProcessHost no longer existing.
  virtual bool SendRequestToNetworkService(
      const GURL& url,
      mojo::PendingRemote<network::mojom::ProxyLookupClient>
          proxy_lookup_client);

  // network::mojom::ProxyLookupClient implementation.
  void OnProxyLookupComplete(
      int32_t net_error,
      const base::Optional<net::ProxyInfo>& proxy_info) override;

  // A PendingRequest is a resolve request that is in progress, or queued.
  struct PendingRequest {
   public:
    PendingRequest(const GURL& url, IPC::Message* reply_msg);
    PendingRequest(PendingRequest&& pending_request) noexcept;
    ~PendingRequest();

    PendingRequest& operator=(PendingRequest&& pending_request) noexcept;

    // The URL of the request.
    GURL url;

    // Data to pass back to the delegate on completion (we own it until then).
    std::unique_ptr<IPC::Message> reply_msg;

   private:
    DISALLOW_COPY_AND_ASSIGN(PendingRequest);
  };

  const int render_process_host_id_;

  // FIFO queue of pending requests. The first entry is always the current one.
  using PendingRequestList = base::circular_deque<PendingRequest>;
  PendingRequestList pending_requests_;

  // Self-reference. Owned as long as there's an outstanding proxy lookup.
  // Needed to shut down safely, since this class is refcounted, with some
  // references owned on multiple threads, while |receiver_| lives on the UI
  // thread, and may receive callbacks there whenever there's a pending request.
  scoped_refptr<ResolveProxyMsgHelper> owned_self_;

  // Receiver for the currently in-progress request, if any.
  mojo::Receiver<network::mojom::ProxyLookupClient> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(ResolveProxyMsgHelper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RESOLVE_PROXY_MSG_HELPER_H_
