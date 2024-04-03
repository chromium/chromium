// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NETWORK_TRUST_TOKEN_BROWSERTEST_H_
#define CONTENT_BROWSER_NETWORK_TRUST_TOKEN_BROWSERTEST_H_

#include <string_view>

#include "base/memory/raw_ref.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/trust_token_access_details.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/content_browser_test.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/test/trust_token_request_handler.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/public/browser/android/java_interfaces.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace content {

using network::test::TrustTokenRequestHandler;

// TrustTokenBrowsertest is a fixture containing boilerplate for initializing an
// HTTPS test server and passing requests through to an embedded instance of
// network::test::TrustTokenRequestHandler, which contains the guts of the
// "server-side" token issuance and redemption logic as well as some consistency
// checks for subsequent signed requests.
//
// The virtual inheritance is necessary as DevtoolsTrustTokenBrowsertest builds
// an inheritance diamond with ContentBrowserTest at the root. Any class in the
// diamond inheriting from ContentBrowserTest directly, needs to do so
// virtually. Otherwise DevtoolsTrustTokenBrowsertest would contain multiple
// copies of ContentBrowserTest's members.
class TrustTokenBrowsertest : virtual public ContentBrowserTest,
                              public WebContentsObserver {
 public:
  TrustTokenBrowsertest();

  // Registers the following handlers:
  // - default //content/test/data files;
  // - a special "/issue" endpoint executing Trust Tokens issuance;
  // - a special "/redeem" endpoint executing redemption; and
  // - a special "/sign" endpoint that verifies that the received signed request
  // data is correctly structured and that the provided Sec-Signature header's
  // verification key was previously bound to a successful token redemption.
  void SetUpOnMainThread() override;

  void OnTrustTokensAccessed(RenderFrameHost* render_frame_host,
                             const TrustTokenAccessDetails& details) override;

  void OnTrustTokensAccessed(NavigationHandle* navigation_handle,
                             const TrustTokenAccessDetails& details) override;

 protected:
  // Provides the network service key commitments from the internal
  // TrustTokenRequestHandler. All hosts in |hosts| will be provided identical
  // commitments.
  void ProvideRequestHandlerKeyCommitmentsToNetworkService(
      std::vector<std::string_view> hosts = {});

  // Given a host (e.g. "a.test"), returns the corresponding storage origin
  // for Trust Tokens state. (This adds the correct scheme---probably https---as
  // well as |server_|'s port, which can vary from test to test. There's no
  // ambiguity in the result because the scheme and port are both fixed across
  // all domains.)
  std::string IssuanceOriginFromHost(const std::string& host) const;

  base::test::ScopedFeatureList features_;

  // TODO(davidvc): Extend this to support more than one key set.
  TrustTokenRequestHandler request_handler_;

  net::EmbeddedTestServer server_{net::EmbeddedTestServer::TYPE_HTTPS};

  // Number of accesses reported by the Trust Token observer.
  int access_count_ = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_NETWORK_TRUST_TOKEN_BROWSERTEST_H_
