// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/mixed_content_navigation_throttle.h"

#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/navigation_simulator_impl.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class MixedContentNavigationThrottleTest : public RenderViewHostTestHarness {};

// Checks that when the throttle observes a subframe navigation loaded with a
// certificate error, the navigation entry is updated.
TEST_F(MixedContentNavigationThrottleTest, HandleCertificateError) {
  auto nav_simulator = NavigationSimulatorImpl::CreateBrowserInitiated(
      GURL("https://example1.test/"), web_contents());
  net::SSLInfo main_frame_ssl_info;
  main_frame_ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  nav_simulator->SetSSLInfo(main_frame_ssl_info);
  nav_simulator->Start();
  nav_simulator->ReadyToCommit();
  nav_simulator->Commit();

  RenderFrameHost* subframe =
      RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  auto subframe_nav_simulator =
      NavigationSimulatorImpl::CreateRendererInitiated(
          GURL("https://example2.test/"), subframe);
  net::SSLInfo subframe_ssl_info;
  subframe_ssl_info.cert_status = net::ERR_CERT_DATE_INVALID;
  subframe_nav_simulator->SetSSLInfo(subframe_ssl_info);
  subframe_nav_simulator->Start();
  subframe_nav_simulator->ReadyToCommit();
  subframe_nav_simulator->Commit();
  EXPECT_TRUE(web_contents()
                  ->GetController()
                  .GetLastCommittedEntry()
                  ->GetSSL()
                  .content_status &
              SSLStatus::RAN_CONTENT_WITH_CERT_ERRORS);
}

// Checks that when the throttle observes a subframe navigation loaded with a
// redirect through a host that has a certificate error, the navigation entry is
// updated.
TEST_F(MixedContentNavigationThrottleTest, HandleCertificateErrorRedirect) {
  auto nav_simulator = NavigationSimulatorImpl::CreateBrowserInitiated(
      GURL("https://example1.test/"), web_contents());
  net::SSLInfo main_frame_ssl_info;
  main_frame_ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  nav_simulator->SetSSLInfo(main_frame_ssl_info);
  nav_simulator->Start();
  nav_simulator->ReadyToCommit();
  nav_simulator->Commit();

  RenderFrameHost* subframe =
      RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  auto subframe_nav_simulator =
      NavigationSimulatorImpl::CreateRendererInitiated(
          GURL("https://example2.test/"), subframe);
  net::SSLInfo subframe_ssl_info;
  subframe_ssl_info.cert_status = net::ERR_CERT_DATE_INVALID;
  subframe_nav_simulator->SetSSLInfo(subframe_ssl_info);
  subframe_nav_simulator->Start();
  subframe_nav_simulator->Redirect(GURL("https://example3.test/"));
  EXPECT_TRUE(web_contents()
                  ->GetController()
                  .GetLastCommittedEntry()
                  ->GetSSL()
                  .content_status &
              SSLStatus::RAN_CONTENT_WITH_CERT_ERRORS);
}

// Tests that MixedContentNavigationThrottle correctly detects or ignores many
// cases where there is or there is not mixed content, respectively.
// Note: Browser side version of MixedContentCheckerTest.IsMixedContent. Must be
// kept in sync manually!
TEST_F(MixedContentNavigationThrottleTest, IsMixedContent) {
  struct TestCase {
    const char* origin;
    const char* target;
    const bool expected_mixed_content;
  } cases[] = {
      {"http://example.com/foo", "http://example.com/foo", false},
      {"http://example.com/foo", "https://example.com/foo", false},
      {"http://example.com/foo", "data:text/html,<p>Hi!</p>", false},
      {"http://example.com/foo", "about:blank", false},
      {"https://example.com/foo", "https://example.com/foo", false},
      {"https://example.com/foo", "wss://example.com/foo", false},
      {"https://example.com/foo", "data:text/html,<p>Hi!</p>", false},
      {"https://example.com/foo", "blob:https://example.com/foo", false},
      {"https://example.com/foo", "filesystem:https://example.com/foo", false},
      {"https://example.com/foo", "http://127.0.0.1/", false},
      {"https://example.com/foo", "http://[::1]/", false},
      {"https://example.com/foo", "http://a.localhost/", false},
      {"https://example.com/foo", "http://localhost/", false},

      {"https://example.com/foo", "http://example.com/foo", true},
      {"https://example.com/foo", "http://google.com/foo", true},
      {"https://example.com/foo", "ws://example.com/foo", true},
      {"https://example.com/foo", "ws://google.com/foo", true},
      {"https://example.com/foo", "http://192.168.1.1/", true},
      {"https://example.com/foo", "blob:http://example.com/foo", true},
      {"https://example.com/foo", "blob:null/foo", true},
      {"https://example.com/foo", "filesystem:http://example.com/foo", true},
      {"https://example.com/foo", "filesystem:null/foo", true},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(::testing::Message()
                 << "Origin: " << test.origin << ", Target: " << test.target
                 << ", Expectation: " << test.expected_mixed_content);
    GURL origin_url(test.origin);
    GURL target_url(test.target);
    EXPECT_EQ(test.expected_mixed_content,
              MixedContentNavigationThrottle::IsMixedContentForTesting(
                  origin_url, target_url));
  }
}

}  // namespace content
