// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/content_index/content_index_service_impl.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/test_support/fake_message_dispatch_context.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

SkBitmap CreateIcon(int resolution) {
  SkBitmap icon;
  icon.allocN32Pixels(1, resolution);
  return icon;
}

class ContentIndexServiceImplTest : public ::testing::Test {
 public:
  static constexpr char kOrigin[] = "https://example.com";

  ContentIndexServiceImplTest()
      : service_(std::make_unique<ContentIndexServiceImpl>(
            url::Origin::Create(GURL(kOrigin)),
            /* content_index_context= */ nullptr,
            /* is_top_level_context= */ true)) {}

  ContentIndexServiceImplTest(const ContentIndexServiceImplTest&) = delete;
  ContentIndexServiceImplTest& operator=(const ContentIndexServiceImplTest&) =
      delete;

  ~ContentIndexServiceImplTest() override = default;

  void Add(const SkBitmap& icon, const GURL& launch_url) {
    base::RunLoop run_loop;
    service_->Add(
        /* service_worker_registration_id= */ 42, /* description= */ nullptr,
        {icon}, launch_url,
        base::BindLambdaForTesting([&](blink::mojom::ContentIndexError error) {
          EXPECT_EQ(error, blink::mojom::ContentIndexError::INVALID_PARAMETER);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  mojo::test::BadMessageObserver& bad_message_observer() {
    return bad_message_observer_;
  }

 private:
  BrowserTaskEnvironment task_environment_;  // Must be first member
  std::unique_ptr<ContentIndexServiceImpl> service_;
  mojo::FakeMessageDispatchContext fake_dispatch_context_;
  mojo::test::BadMessageObserver bad_message_observer_;
};

// static
constexpr char ContentIndexServiceImplTest::kOrigin[];

TEST_F(ContentIndexServiceImplTest, NullIcon) {
  Add(SkBitmap(), GURL(kOrigin));
  EXPECT_EQ("Invalid icon", bad_message_observer().WaitForBadMessage());
}

TEST_F(ContentIndexServiceImplTest, LargeIcon) {
  Add(CreateIcon(/* resolution= */ 2 *
                 blink::mojom::ContentIndexService::kMaxIconResolution),
      GURL(kOrigin));
  EXPECT_EQ("Invalid icon", bad_message_observer().WaitForBadMessage());
}

TEST_F(ContentIndexServiceImplTest, InvalidLaunchUrl) {
  Add(CreateIcon(/* resolution= */ 0.5 *
                 blink::mojom::ContentIndexService::kMaxIconResolution),
      GURL());
  EXPECT_EQ("Invalid launch URL", bad_message_observer().WaitForBadMessage());
}

TEST_F(ContentIndexServiceImplTest, CrossOriginLaunchUrl) {
  Add(CreateIcon(/* resolution= */ 0.5 *
                 blink::mojom::ContentIndexService::kMaxIconResolution),
      GURL("https://evil.com"));
  EXPECT_EQ("Invalid launch URL", bad_message_observer().WaitForBadMessage());
}

}  // namespace
}  // namespace content
