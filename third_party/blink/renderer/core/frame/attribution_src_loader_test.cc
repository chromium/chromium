// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/attribution_src_loader.h"

#include <stddef.h>

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

namespace {

using blink::url_test_helpers::RegisterMockedErrorURLLoad;
using blink::url_test_helpers::RegisterMockedURLLoad;
using blink::url_test_helpers::ToKURL;

}  // namespace

class AttributionSrcLoaderTest : public testing::Test {
 public:
  void SetUp() override {
    dummy_page_holder_ = std::make_unique<DummyPageHolder>();

    SecurityContext& security_context = dummy_page_holder_->GetDocument()
                                            .GetFrame()
                                            ->DomWindow()
                                            ->GetSecurityContext();
    security_context.SetSecurityOriginForTesting(nullptr);
    security_context.SetSecurityOrigin(
        SecurityOrigin::CreateFromString("https://example.com"));

    attribution_src_loader_ = MakeGarbageCollected<AttributionSrcLoader>(
        dummy_page_holder_->GetDocument().GetFrame());
  }

  void TearDown() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

 protected:
  Persistent<AttributionSrcLoader> attribution_src_loader_;
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

TEST_F(AttributionSrcLoaderTest, AttributionSrcRequestStatusHistogram) {
  base::HistogramTester histograms;

  KURL url1 = ToKURL("https://example1.com/foo.html");
  RegisterMockedURLLoad(url1, test::CoreTestDataPath("foo.html"));

  attribution_src_loader_->Register(url1, /*element=*/nullptr);

  KURL url2 = ToKURL("https://example2.com/foo.html");
  RegisterMockedErrorURLLoad(url2);

  attribution_src_loader_->Register(url2, /*element=*/nullptr);

  // kRequested = 0.
  histograms.ExpectUniqueSample("Conversions.AttributionSrcRequestStatus", 0,
                                2);

  url_test_helpers::ServeAsynchronousRequests();

  // kReceived = 1.
  histograms.ExpectBucketCount("Conversions.AttributionSrcRequestStatus", 1, 1);

  // kFailed = 2.
  histograms.ExpectBucketCount("Conversions.AttributionSrcRequestStatus", 2, 1);
}

TEST_F(AttributionSrcLoaderTest, TooManyConcurrentRequests_NewRequestDropped) {
  KURL url = ToKURL("https://example1.com/foo.html");
  RegisterMockedURLLoad(url, test::CoreTestDataPath("foo.html"));

  for (size_t i = 0; i < AttributionSrcLoader::kMaxConcurrentRequests; ++i) {
    EXPECT_EQ(attribution_src_loader_->RegisterSources(url),
              AttributionSrcLoader::RegisterResult::kSuccess);
  }

  EXPECT_EQ(attribution_src_loader_->RegisterSources(url),
            AttributionSrcLoader::RegisterResult::kFailedToRegister);

  url_test_helpers::ServeAsynchronousRequests();

  EXPECT_EQ(attribution_src_loader_->RegisterSources(url),
            AttributionSrcLoader::RegisterResult::kSuccess);
}

}  // namespace blink
