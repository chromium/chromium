// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/threaded_icon_loader.h"

#include "base/optional.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/histogram_tester.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace {

constexpr char kIconLoaderBaseUrl[] = "http://test.com/";
constexpr char kIconLoaderBaseDir[] = "notifications/";
constexpr char kIconLoaderIcon100x100[] = "100x100.png";
constexpr char kIconLoaderInvalidIcon[] = "file.txt";

class ThreadedIconLoaderTest : public PageTestBase {
 public:
  void SetUp() override {
    PageTestBase::SetUp(IntSize());
    GetDocument().SetBaseURLOverride(KURL(kIconLoaderBaseUrl));
  }

  void TearDown() override {
    platform_->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();
  }

  // Registers a mocked url. When fetched, |fileName| will be loaded from the
  // test data directory.
  KURL RegisterMockedURL(const String& file_name) {
    return url_test_helpers::RegisterMockedURLLoadFromBase(
        kIconLoaderBaseUrl, test::CoreTestDataPath(kIconLoaderBaseDir),
        file_name, "image/png");
  }

  std::pair<SkBitmap, double> LoadIcon(
      const KURL& url,
      base::Optional<WebSize> resize_dimensions = base::nullopt) {
    auto* icon_loader = MakeGarbageCollected<ThreadedIconLoader>();

    ResourceRequest resource_request(url);
    resource_request.SetRequestContext(mojom::RequestContextType::IMAGE);
    resource_request.SetPriority(ResourceLoadPriority::kMedium);

    SkBitmap icon;
    double resize_scale;
    base::RunLoop run_loop;
    icon_loader->Start(
        &GetDocument(), resource_request, resize_dimensions,
        WTF::Bind(&ThreadedIconLoaderTest::DidGetIcon, WTF::Unretained(this),
                  run_loop.QuitClosure(), WTF::Unretained(&icon),
                  WTF::Unretained(&resize_scale)));
    platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
    run_loop.Run();

    return {icon, resize_scale};
  }

 private:
  void DidGetIcon(base::OnceClosure quit_closure,
                  SkBitmap* out_icon,
                  double* out_resize_scale,
                  SkBitmap icon,
                  double resize_scale) {
    *out_icon = std::move(icon);
    *out_resize_scale = resize_scale;
    std::move(quit_closure).Run();
  }

  ScopedTestingPlatformSupport<TestingPlatformSupport> platform_;
};

TEST_F(ThreadedIconLoaderTest, LoadIcon) {
  auto result = LoadIcon(RegisterMockedURL(kIconLoaderIcon100x100));
  const SkBitmap& icon = result.first;
  double resize_scale = result.second;

  ASSERT_FALSE(icon.isNull());
  EXPECT_FALSE(icon.drawsNothing());
  EXPECT_EQ(icon.width(), 100);
  EXPECT_EQ(icon.height(), 100);
  EXPECT_EQ(resize_scale, 1.0);
}

TEST_F(ThreadedIconLoaderTest, LoadAndDownscaleIcon) {
  WebSize dimensions = {50, 50};
  auto result = LoadIcon(RegisterMockedURL(kIconLoaderIcon100x100), dimensions);
  const SkBitmap& icon = result.first;
  double resize_scale = result.second;

  ASSERT_FALSE(icon.isNull());
  EXPECT_FALSE(icon.drawsNothing());
  EXPECT_EQ(icon.width(), 50);
  EXPECT_EQ(icon.height(), 50);
  EXPECT_EQ(resize_scale, 0.5);
}

TEST_F(ThreadedIconLoaderTest, LoadIconAndUpscaleIgnored) {
  WebSize dimensions = {500, 500};
  auto result = LoadIcon(RegisterMockedURL(kIconLoaderIcon100x100), dimensions);
  const SkBitmap& icon = result.first;
  double resize_scale = result.second;

  ASSERT_FALSE(icon.isNull());
  EXPECT_FALSE(icon.drawsNothing());
  EXPECT_EQ(icon.width(), 100);
  EXPECT_EQ(icon.height(), 100);
  EXPECT_EQ(resize_scale, 1.0);
}

TEST_F(ThreadedIconLoaderTest, InvalidResourceReturnsNullIcon) {
  auto result = LoadIcon(RegisterMockedURL(kIconLoaderInvalidIcon));
  const SkBitmap& icon = result.first;
  double resize_scale = result.second;

  ASSERT_TRUE(icon.isNull());
  EXPECT_EQ(resize_scale, -1.0);
}

TEST_F(ThreadedIconLoaderTest, LoadTimeRecordedByUMA) {
  HistogramTester histogram_tester;
  LoadIcon(RegisterMockedURL(kIconLoaderIcon100x100));
  histogram_tester.ExpectTotalCount("Blink.ThreadedIconLoader.LoadTime", 1);
}

TEST_F(ThreadedIconLoaderTest, ResizeFailed) {
  WebSize dimensions = {25, 0};
  auto result = LoadIcon(RegisterMockedURL(kIconLoaderIcon100x100), dimensions);
  const SkBitmap& icon = result.first;
  double resize_scale = result.second;

  // Resizing should have failed so the original will be returned.
  ASSERT_FALSE(icon.isNull());
  EXPECT_EQ(icon.width(), 100);
  EXPECT_EQ(icon.height(), 100);
  EXPECT_EQ(resize_scale, 1.0);
}

}  // namespace
}  // namespace blink
