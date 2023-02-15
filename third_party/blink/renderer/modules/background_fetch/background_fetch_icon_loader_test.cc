// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/background_fetch/background_fetch_icon_loader.h"

#include <utility>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_image_resource.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {
namespace {

enum class BackgroundFetchLoadState {
  kNotLoaded,
  kLoadFailed,
  kLoadSuccessful
};

constexpr char kBackgroundFetchImageLoaderBaseUrl[] = "http://test.com/";
constexpr char kBackgroundFetchImageLoaderBaseDir[] = "notifications/";
constexpr char kBackgroundFetchImageLoaderIcon500x500FullPath[] =
    "http://test.com/500x500.png";
constexpr char kBackgroundFetchImageLoaderIcon500x500[] = "500x500.png";
constexpr char kBackgroundFetchImageLoaderIcon48x48[] = "48x48.png";
constexpr char kBackgroundFetchImageLoaderIcon3000x2000[] = "3000x2000.png";

}  // namespace

class BackgroundFetchIconLoaderTest : public PageTestBase {
 public:
  BackgroundFetchIconLoaderTest()
      : loader_(MakeGarbageCollected<BackgroundFetchIconLoader>()) {}
  ~BackgroundFetchIconLoaderTest() override {
    loader_->Stop();
    URLLoaderMockFactory::GetSingletonInstance()
        ->UnregisterAllURLsAndClearMemoryCache();
  }

  void SetUp() override {
    PageTestBase::SetUp(gfx::Size());
    GetDocument().SetBaseURLOverride(KURL(kBackgroundFetchImageLoaderBaseUrl));
    RegisterMockedURL(kBackgroundFetchImageLoaderIcon500x500);
    RegisterMockedURL(kBackgroundFetchImageLoaderIcon48x48);
    RegisterMockedURL(kBackgroundFetchImageLoaderIcon3000x2000);
  }

  // Registers a mocked URL.
  WebURL RegisterMockedURL(const String& file_name) {
    WebURL registered_url = url_test_helpers::RegisterMockedURLLoadFromBase(
        kBackgroundFetchImageLoaderBaseUrl,
        test::CoreTestDataPath(kBackgroundFetchImageLoaderBaseDir), file_name,
        "image/png");
    return registered_url;
  }

  // Callback for BackgroundFetchIconLoader. This will set up the state of the
  // load as either success or failed based on whether the bitmap is empty.
  void IconLoaded(base::OnceClosure quit_closure,
                  const SkBitmap& bitmap,
                  int64_t ideal_to_chosen_icon_size) {
    bitmap_ = bitmap;

    if (!bitmap_.isNull())
      loaded_ = BackgroundFetchLoadState::kLoadSuccessful;
    else
      loaded_ = BackgroundFetchLoadState::kLoadFailed;

    std::move(quit_closure).Run();
  }

  ManifestImageResource* CreateTestIcon(const String& url_str,
                                        const String& size) {
    ManifestImageResource* icon = ManifestImageResource::Create();
    icon->setSrc(url_str);
    icon->setType("image/png");
    icon->setSizes(size);
    icon->setPurpose("any");
    return icon;
  }

  KURL PickRightIcon(HeapVector<Member<ManifestImageResource>> icons,
                     const gfx::Size& ideal_display_size) {
    loader_->icons_ = std::move(icons);

    return loader_->PickBestIconForDisplay(GetContext(),
                                           ideal_display_size.height());
  }

  void LoadIcon(const KURL& url,
                const gfx::Size& maximum_size,
                base::OnceClosure quit_closure,
                const String& sizes = "500x500",
                const String& purpose = "ANY") {
    ManifestImageResource* icon = ManifestImageResource::Create();
    icon->setSrc(url.GetString());
    icon->setType("image/png");
    icon->setSizes(sizes);
    icon->setPurpose(purpose);
    HeapVector<Member<ManifestImageResource>> icons(1, icon);
    loader_->icons_ = std::move(icons);
    loader_->DidGetIconDisplaySizeIfSoLoadIcon(
        GetContext(),
        WTF::BindOnce(&BackgroundFetchIconLoaderTest::IconLoaded,
                      WTF::Unretained(this), std::move(quit_closure)),
        maximum_size);
  }

  ExecutionContext* GetContext() const { return GetFrame().DomWindow(); }

 protected:
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform_;
  BackgroundFetchLoadState loaded_ = BackgroundFetchLoadState::kNotLoaded;
  SkBitmap bitmap_;

 private:
  Persistent<BackgroundFetchIconLoader> loader_;
};

TEST_F(BackgroundFetchIconLoaderTest, SuccessTest) {
  base::RunLoop run_loop;

  gfx::Size maximum_size{192, 168};
  LoadIcon(KURL(kBackgroundFetchImageLoaderIcon500x500FullPath), maximum_size,
           run_loop.QuitClosure());

  URLLoaderMockFactory::GetSingletonInstance()->ServeAsynchronousRequests();

  run_loop.Run();

  ASSERT_EQ(BackgroundFetchLoadState::kLoadSuccessful, loaded_);
  ASSERT_FALSE(bitmap_.drawsNothing());

  // Resizing a 500x500 image to fit on a canvas of 192x168 pixels should yield
  // a decoded image size of 168x168, avoiding image data to get lost.
  EXPECT_EQ(bitmap_.width(), 168);
  EXPECT_EQ(bitmap_.height(), 168);
}

TEST_F(BackgroundFetchIconLoaderTest, PickIconRelativePath) {
  HeapVector<Member<ManifestImageResource>> icons;
  icons.push_back(
      CreateTestIcon(kBackgroundFetchImageLoaderIcon500x500, "500x500"));

  KURL best_icon = PickRightIcon(std::move(icons), gfx::Size(500, 500));
  ASSERT_TRUE(best_icon.IsValid());
  EXPECT_EQ(best_icon, KURL(kBackgroundFetchImageLoaderIcon500x500FullPath));
}

TEST_F(BackgroundFetchIconLoaderTest, PickIconFullPath) {
  HeapVector<Member<ManifestImageResource>> icons;
  icons.push_back(CreateTestIcon(kBackgroundFetchImageLoaderIcon500x500FullPath,
                                 "500x500"));

  KURL best_icon = PickRightIcon(std::move(icons), gfx::Size(500, 500));
  ASSERT_TRUE(best_icon.IsValid());
  EXPECT_EQ(best_icon, KURL(kBackgroundFetchImageLoaderIcon500x500FullPath));
}

TEST_F(BackgroundFetchIconLoaderTest, PickRightIcon) {
  ManifestImageResource* icon0 =
      CreateTestIcon(kBackgroundFetchImageLoaderIcon500x500, "500x500");
  ManifestImageResource* icon1 =
      CreateTestIcon(kBackgroundFetchImageLoaderIcon48x48, "48x48");
  ManifestImageResource* icon2 =
      CreateTestIcon(kBackgroundFetchImageLoaderIcon3000x2000, "3000x2000");

  HeapVector<Member<ManifestImageResource>> icons;
  icons.push_back(icon0);
  icons.push_back(icon1);
  icons.push_back(icon2);

  KURL best_icon = PickRightIcon(std::move(icons), gfx::Size(42, 42));
  ASSERT_TRUE(best_icon.IsValid());
  // We expect the smallest Icon larger than the ideal display size.
  EXPECT_EQ(best_icon, KURL(KURL(kBackgroundFetchImageLoaderBaseUrl),
                            kBackgroundFetchImageLoaderIcon48x48));
}

TEST_F(BackgroundFetchIconLoaderTest, EmptySizes) {
  base::RunLoop run_loop;

  gfx::Size maximum_size{192, 168};
  LoadIcon(KURL(kBackgroundFetchImageLoaderIcon500x500FullPath), maximum_size,
           run_loop.QuitClosure(), "", "ANY");

  URLLoaderMockFactory::GetSingletonInstance()->ServeAsynchronousRequests();

  run_loop.Run();

  ASSERT_EQ(BackgroundFetchLoadState::kLoadSuccessful, loaded_);
  ASSERT_FALSE(bitmap_.drawsNothing());
}

TEST_F(BackgroundFetchIconLoaderTest, EmptyPurpose) {
  base::RunLoop run_loop;

  gfx::Size maximum_size{192, 168};
  LoadIcon(KURL(kBackgroundFetchImageLoaderIcon500x500FullPath), maximum_size,
           run_loop.QuitClosure(), "500X500", "");

  URLLoaderMockFactory::GetSingletonInstance()->ServeAsynchronousRequests();

  run_loop.Run();

  ASSERT_EQ(BackgroundFetchLoadState::kLoadSuccessful, loaded_);
  ASSERT_FALSE(bitmap_.drawsNothing());
}

}  // namespace blink
