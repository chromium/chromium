// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_PROXY_LOOKUP_HELPER_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_PROXY_LOOKUP_HELPER_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/proxy_lookup_client.mojom.h"

class GURL;

namespace net {
class ProxyInfo;
}

namespace content {

// Class that runs a single proxy resolution on the UI thread. Lives on the
// thread its created on, and uses a helper to run tasks off-thread. Can be
// destroyed at any time.
class CONTENT_EXPORT PepperProxyLookupHelper {
 public:
  // Callback to call LookUpProxyForURL. Called on the UI thread. Needed for
  // testing. Returns false if unable to make the call, for whatever reason.
  using LookUpProxyForURLCallback = base::OnceCallback<bool(
      const GURL& url,
      mojo::PendingRemote<network::mojom::ProxyLookupClient>
          proxy_lookup_client)>;

  // Callback to invoke when complete. Invoked on thread the
  // PepperProxyLookupHelper was created on.
  using LookUpCompleteCallback =
      base::OnceCallback<void(std::optional<net::ProxyInfo> proxy_info)>;

  PepperProxyLookupHelper();

  PepperProxyLookupHelper(const PepperProxyLookupHelper&) = delete;
  PepperProxyLookupHelper& operator=(const PepperProxyLookupHelper&) = delete;

  ~PepperProxyLookupHelper();

  // Starts a lookup for |url| on the UI thread. Invokes
  // |look_up_proxy_for_url_callback| on the UI thread to start the lookup, and
  // calls |look_up_complete_callback| on the thread Start() was called on when
  // complete. May only be invoked once.
  void Start(const GURL& url,
             LookUpProxyForURLCallback look_up_proxy_for_url_callback,
             LookUpCompleteCallback look_up_complete_callback);

 private:
  class UIThreadHelper;

  void OnProxyLookupComplete(std::optional<net::ProxyInfo> proxy_info);

  LookUpCompleteCallback look_up_complete_callback_;
  std::unique_ptr<UIThreadHelper> ui_thread_helper_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PepperProxyLookupHelper> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_PROXY_LOOKUP_HELPER_H_
