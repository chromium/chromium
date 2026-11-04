// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/utils/page_context_util.h"

#import "base/apple/foundation_util.h"
#import "base/base64.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/fake_page_context_wrapper.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/fake_page_context_wrapper_factory.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper_config.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"

namespace {

constexpr base::TimeDelta kTimeout = base::Seconds(30);

}  // namespace

// Test fixture for PageContextUtil.
class PageContextUtilTest : public PlatformTest {
 public:
  PageContextUtilTest() : web_state_(std::make_unique<web::FakeWebState>()) {}
  std::unique_ptr<web::FakeWebState> web_state_;
};

TEST_F(PageContextUtilTest, PopulateWhenNotLoading) {
  FakePageContextWrapper* fake_wrapper = [[FakePageContextWrapper alloc]
        initWithWebState:web_state_.get()
                  config:PageContextWrapperConfigBuilder().Build()
      completionCallback:base::DoNothing()];

  web_state_->SetLoading(false);
  PopulatePageContext(fake_wrapper, web_state_.get());

  EXPECT_TRUE(fake_wrapper.populateCalled);
  EXPECT_FALSE(fake_wrapper.timeout.has_value());
}

TEST_F(PageContextUtilTest, PopulateWhenLoading) {
  FakePageContextWrapper* fake_wrapper = [[FakePageContextWrapper alloc]
        initWithWebState:web_state_.get()
                  config:PageContextWrapperConfigBuilder().Build()
      completionCallback:base::DoNothing()];

  web_state_->SetLoading(true);
  PopulatePageContext(fake_wrapper, web_state_.get());

  EXPECT_FALSE(fake_wrapper.populateCalled);

  web_state_->OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  EXPECT_TRUE(fake_wrapper.populateCalled);
  EXPECT_FALSE(fake_wrapper.timeout.has_value());
}

TEST_F(PageContextUtilTest, PopulateWhenNotLoadingWithTimeout) {
  FakePageContextWrapper* fake_wrapper = [[FakePageContextWrapper alloc]
        initWithWebState:web_state_.get()
                  config:PageContextWrapperConfigBuilder().Build()
      completionCallback:base::DoNothing()];

  web_state_->SetLoading(false);
  PopulatePageContextWithTimeout(fake_wrapper, web_state_.get(), kTimeout);

  EXPECT_TRUE(fake_wrapper.populateCalled);
  EXPECT_EQ(fake_wrapper.timeout, kTimeout);
}

TEST_F(PageContextUtilTest, PopulateWhenLoadingWithTimeout) {
  FakePageContextWrapper* fake_wrapper = [[FakePageContextWrapper alloc]
        initWithWebState:web_state_.get()
                  config:PageContextWrapperConfigBuilder().Build()
      completionCallback:base::DoNothing()];

  web_state_->SetLoading(true);
  PopulatePageContextWithTimeout(fake_wrapper, web_state_.get(), kTimeout);

  web_state_->OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);

  EXPECT_TRUE(fake_wrapper.populateCalled);
  EXPECT_EQ(fake_wrapper.timeout, kTimeout);
}

TEST_F(PageContextUtilTest, SaveAndLoadPageContext) {
  optimization_guide::proto::PageContext page_context;
  page_context.set_url("https://www.example.com");
  page_context.set_title("Example");

  SavePageContextResult result = SaveSerializedPageContextToDisk(page_context);
  EXPECT_TRUE(result.success);
  EXPECT_FALSE(result.file_path.empty());
  EXPECT_TRUE(base::PathExists(result.file_path));

  std::string file_content;
  EXPECT_TRUE(base::ReadFileToString(result.file_path, &file_content));

  optimization_guide::proto::PageContext loaded_context;
  EXPECT_TRUE(loaded_context.ParseFromString(file_content));

  EXPECT_EQ(page_context.url(), loaded_context.url());
  EXPECT_EQ(page_context.title(), loaded_context.title());

  // Clean up
  EXPECT_TRUE(base::DeleteFile(result.file_path));
}

TEST_F(PageContextUtilTest, SaveAndLoadPageContextWithScreenshot) {
  optimization_guide::proto::PageContext page_context;
  page_context.set_url("https://www.example.com");
  page_context.set_title("Example");
  std::string dummy_png = "dummy_png_data";
  page_context.set_tab_screenshot(base::Base64Encode(dummy_png));

  SavePageContextResult result = SaveSerializedPageContextToDisk(page_context);
  EXPECT_TRUE(result.success);
  EXPECT_FALSE(result.file_path.empty());
  EXPECT_TRUE(base::PathExists(result.file_path));
  ASSERT_TRUE(result.screenshot_file_path.has_value());
  EXPECT_TRUE(base::PathExists(*result.screenshot_file_path));

  std::string screenshot_content;
  EXPECT_TRUE(base::ReadFileToString(*result.screenshot_file_path,
                                     &screenshot_content));
  EXPECT_EQ(dummy_png, screenshot_content);

  // Clean up
  EXPECT_TRUE(base::DeleteFile(result.file_path));
  EXPECT_TRUE(base::DeleteFile(*result.screenshot_file_path));
}

TEST_F(PageContextUtilTest, CreatePageContextWrapperWithRichExtraction) {
  FakePageContextWrapperFactory factory;
  FakePageContextWrapper* fake_wrapper =
      base::apple::ObjCCastStrict<FakePageContextWrapper>(
          CreatePageContextWrapper(web_state_.get(), true, base::DoNothing(),
                                   &factory));

  EXPECT_TRUE(fake_wrapper.config.use_rich_extraction());
  EXPECT_TRUE(fake_wrapper.config.use_refactored_extractor());
  EXPECT_TRUE(fake_wrapper.config.graft_cross_origin_frame_content());
  EXPECT_TRUE(fake_wrapper.config.extract_paid_content());
  EXPECT_TRUE(fake_wrapper.shouldGetAnnotatedPageContent);
  EXPECT_TRUE(fake_wrapper.shouldGetSnapshot);
  EXPECT_TRUE(fake_wrapper.shouldGetFullPagePDF);
}

TEST_F(PageContextUtilTest, CreatePageContextWrapperWithoutRichExtraction) {
  FakePageContextWrapperFactory factory;
  FakePageContextWrapper* fake_wrapper =
      base::apple::ObjCCastStrict<FakePageContextWrapper>(
          CreatePageContextWrapper(web_state_.get(), false, base::DoNothing(),
                                   &factory));

  EXPECT_FALSE(fake_wrapper.config.use_rich_extraction());
  EXPECT_TRUE(fake_wrapper.shouldGetAnnotatedPageContent);
  EXPECT_TRUE(fake_wrapper.shouldGetSnapshot);
  EXPECT_TRUE(fake_wrapper.shouldGetFullPagePDF);
}
