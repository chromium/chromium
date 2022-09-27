// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/cookie_jar.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {
namespace {

class CookieJarTest : public testing::Test {
 public:
  CookieJarTest() {
    url_test_helpers::RegisterMockedURLLoad(
        url_test_helpers::ToKURL("https://example.com/hello_world.html"),
        test::CoreTestDataPath("hello_world.html"));
    web_view_helper_.InitializeAndLoad("https://example.com/hello_world.html");
  }

  ~CookieJarTest() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

  Document* GetDocument() {
    return web_view_helper_.LocalMainFrame()->GetFrame()->GetDocument();
  }

 private:
  frame_test_helpers::WebViewHelper web_view_helper_;
};

TEST_F(CookieJarTest, SetCookieHistogram) {
  V8TestingScope scope;
  {
    base::HistogramTester histogram;
    GetDocument()->setCookie("foo=bar", scope.GetExceptionState());
    histogram.ExpectTotalCount("Blink.SetCookieTime.ManagerRequested", 1);
    histogram.ExpectTotalCount("Blink.SetCookieTime.ManagerAvailable", 0);
  }
  {
    base::HistogramTester histogram;
    GetDocument()->setCookie("foo=bar", scope.GetExceptionState());
    histogram.ExpectTotalCount("Blink.SetCookieTime.ManagerRequested", 0);
    histogram.ExpectTotalCount("Blink.SetCookieTime.ManagerAvailable", 1);
  }
}

TEST_F(CookieJarTest, CookiesHistogram) {
  V8TestingScope scope;
  {
    base::HistogramTester histogram;
    GetDocument()->cookie(scope.GetExceptionState());
    histogram.ExpectTotalCount("Blink.CookiesTime.ManagerRequested", 1);
    histogram.ExpectTotalCount("Blink.CookiesTime.ManagerAvailable", 0);
  }
  {
    base::HistogramTester histogram;
    GetDocument()->cookie(scope.GetExceptionState());
    histogram.ExpectTotalCount("Blink.CookiesTime.ManagerRequested", 0);
    histogram.ExpectTotalCount("Blink.CookiesTime.ManagerAvailable", 1);
  }
}

TEST_F(CookieJarTest, CookiesEnabledHistogram) {
  {
    base::HistogramTester histogram;
    GetDocument()->CookiesEnabled();
    histogram.ExpectTotalCount("Blink.CookiesEnabledTime.ManagerRequested", 1);
    histogram.ExpectTotalCount("Blink.CookiesEnabledTime.ManagerAvailable", 0);
  }
  {
    base::HistogramTester histogram;
    GetDocument()->CookiesEnabled();
    histogram.ExpectTotalCount("Blink.CookiesEnabledTime.ManagerRequested", 0);
    histogram.ExpectTotalCount("Blink.CookiesEnabledTime.ManagerAvailable", 1);
  }
}

TEST_F(CookieJarTest, CookieTruncatingChar) {
  V8TestingScope scope;
  GetDocument()->setCookie("foo=\0bar", scope.GetExceptionState());
  GetDocument()->IsUseCounted(WebFeature::kCookieWithTruncatingChar);
}

}  // namespace
}  // namespace blink
