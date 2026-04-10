// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "content/browser/webid/webid_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/webid/federated_identity_permission_context_delegate.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/link_header.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class IdpSigninStatusBrowserTest : public ContentBrowserTest {
 public:
  IdpSigninStatusBrowserTest() = default;

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  bool IsIdpSignedIn(const url::Origin& origin) {
    auto* delegate = shell()
                         ->web_contents()
                         ->GetBrowserContext()
                         ->GetFederatedIdentityPermissionContext();
    return delegate->GetIdpSigninStatus(origin).has_value() &&
           *delegate->GetIdpSigninStatus(origin);
  }

  using InterceptorCallback =
      base::RepeatingCallback<bool(URLLoaderInterceptor::RequestParams*)>;

  std::unique_ptr<URLLoaderInterceptor> CreateIdpSigninInterceptor(
      const GURL& idp_url,
      const GURL& rp_url = GURL(),
      InterceptorCallback rp_callback = base::NullCallback()) {
    return std::make_unique<URLLoaderInterceptor>(base::BindRepeating(
        [](const GURL& idp_url, const GURL& rp_url,
           InterceptorCallback rp_callback,
           URLLoaderInterceptor::RequestParams* params) {
          if (!rp_url.is_empty() && params->url_request.url == rp_url) {
            return rp_callback.Run(params);
          }
          if (params->url_request.url == idp_url) {
            URLLoaderInterceptor::WriteResponse(
                "HTTP/1.1 200 OK\nContent-Type: text/html\n"
                "Set-Login: logged-in\n\n",
                "<html><body>IDP</body></html>", params->client.get());
            return true;
          }
          return false;
        },
        idp_url, rp_url, std::move(rp_callback)));
  }

  std::unique_ptr<URLLoaderInterceptor> CreateEarlyHintsInterceptor(
      const GURL& rp_url,
      const GURL& idp_url) {
    return CreateIdpSigninInterceptor(
        idp_url, rp_url,
        base::BindRepeating(
            [](const GURL& idp_url,
               URLLoaderInterceptor::RequestParams* params) {
              auto early_hints = network::mojom::EarlyHints::New();
              early_hints->headers = network::mojom::ParsedHeaders::New();
              auto link = network::mojom::LinkHeader::New();
              link->href = idp_url;
              link->rel = network::mojom::LinkRelAttribute::kPreload;
              link->as = network::mojom::LinkAsAttribute::kScript;
              early_hints->headers->link_headers.push_back(std::move(link));
              params->client->OnReceiveEarlyHints(std::move(early_hints));

              URLLoaderInterceptor::WriteResponse(
                  "HTTP/1.1 200 OK\nContent-Type: text/html\n\n",
                  "<html><body>RP</body></html>", params->client.get());
              return true;
            },
            idp_url));
  }
};

// Test that a cross-origin subresource preload via Early Hints cannot set the
// IdP sign-in status.
IN_PROC_BROWSER_TEST_F(IdpSigninStatusBrowserTest,
                       EarlyHintsCrossOriginSubresource) {
  GURL rp_url("https://rp.test/index.html");
  GURL idp_url("https://idp.test/set-login");
  url::Origin idp_origin = url::Origin::Create(idp_url);

  auto interceptor = CreateEarlyHintsInterceptor(rp_url, idp_url);

  EXPECT_TRUE(NavigateToURL(shell(), rp_url));

  // The IdP sign-in status should NOT be set because it's a cross-origin
  // subresource preload.
  EXPECT_FALSE(IsIdpSignedIn(idp_origin));
}

// Test that a same-origin subresource preload via Early Hints CAN set the
// IdP sign-in status.
IN_PROC_BROWSER_TEST_F(IdpSigninStatusBrowserTest,
                       EarlyHintsSameOriginSubresource) {
  GURL rp_url("https://rp.test/index.html");
  GURL idp_url("https://rp.test/set-login");
  url::Origin idp_origin = url::Origin::Create(idp_url);

  auto interceptor = CreateEarlyHintsInterceptor(rp_url, idp_url);

  EXPECT_TRUE(NavigateToURL(shell(), rp_url));

  // The IdP sign-in status SHOULD be set because it's a same-origin
  // subresource preload.
  EXPECT_TRUE(IsIdpSignedIn(idp_origin));
}

// Test that a top-level navigation CAN set its own IdP sign-in status.
IN_PROC_BROWSER_TEST_F(IdpSigninStatusBrowserTest, TopLevelNavigation) {
  GURL idp_url("https://idp.test/index.html");
  url::Origin idp_origin = url::Origin::Create(idp_url);

  auto interceptor = CreateIdpSigninInterceptor(idp_url);

  EXPECT_TRUE(NavigateToURL(shell(), idp_url));

  // The IdP sign-in status SHOULD be set because it's a top-level navigation.
  EXPECT_TRUE(IsIdpSignedIn(idp_origin));
}

// Test that a regular cross-origin subresource load cannot set the IdP sign-in
// status.
IN_PROC_BROWSER_TEST_F(IdpSigninStatusBrowserTest, CrossOriginSubresource) {
  GURL rp_url("https://rp.test/index.html");
  GURL idp_url("https://idp.test/set-login");
  url::Origin idp_origin = url::Origin::Create(idp_url);

  auto interceptor = CreateIdpSigninInterceptor(
      idp_url, rp_url,
      base::BindRepeating([](URLLoaderInterceptor::RequestParams* params) {
        URLLoaderInterceptor::WriteResponse(
            "HTTP/1.1 200 OK\nContent-Type: text/html\n\n",
            "<html><body>"
            "<script src='https://idp.test/set-login'></script>"
            "</body></html>",
            params->client.get());
        return true;
      }));

  EXPECT_TRUE(NavigateToURL(shell(), rp_url));

  // The IdP sign-in status should NOT be set because it's a cross-origin
  // subresource load.
  EXPECT_FALSE(IsIdpSignedIn(idp_origin));
}

// Test that a same-origin iframe CAN set the IdP sign-in status.
IN_PROC_BROWSER_TEST_F(IdpSigninStatusBrowserTest, SameOriginIframe) {
  GURL rp_url("https://rp.test/index.html");
  GURL idp_url("https://rp.test/iframe.html");
  url::Origin idp_origin = url::Origin::Create(idp_url);

  auto interceptor = CreateIdpSigninInterceptor(
      idp_url, rp_url,
      base::BindRepeating([](URLLoaderInterceptor::RequestParams* params) {
        URLLoaderInterceptor::WriteResponse(
            "HTTP/1.1 200 OK\nContent-Type: text/html\n\n",
            "<html><body>"
            "<iframe src='https://rp.test/iframe.html'></iframe>"
            "</body></html>",
            params->client.get());
        return true;
      }));

  EXPECT_TRUE(NavigateToURL(shell(), rp_url));

  // The IdP sign-in status SHOULD be set because it's a same-origin iframe.
  EXPECT_TRUE(IsIdpSignedIn(idp_origin));
}

// Test that a cross-origin iframe cannot set the IdP sign-in status.
IN_PROC_BROWSER_TEST_F(IdpSigninStatusBrowserTest, CrossOriginIframe) {
  GURL rp_url("https://rp.test/index.html");
  GURL idp_url("https://idp.test/iframe.html");
  url::Origin idp_origin = url::Origin::Create(idp_url);

  auto interceptor = CreateIdpSigninInterceptor(
      idp_url, rp_url,
      base::BindRepeating([](URLLoaderInterceptor::RequestParams* params) {
        URLLoaderInterceptor::WriteResponse(
            "HTTP/1.1 200 OK\nContent-Type: text/html\n\n",
            "<html><body>"
            "<iframe src='https://idp.test/iframe.html'></iframe>"
            "</body></html>",
            params->client.get());
        return true;
      }));

  EXPECT_TRUE(NavigateToURL(shell(), rp_url));

  // The IdP sign-in status should NOT be set because it's a cross-origin
  // iframe.
  EXPECT_FALSE(IsIdpSignedIn(idp_origin));
}

}  // namespace content
