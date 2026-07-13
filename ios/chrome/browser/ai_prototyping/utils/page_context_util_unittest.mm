// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/utils/page_context_util.h"

#import "base/files/file_util.h"
#import "base/functional/callback_helpers.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper_config.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

constexpr base::TimeDelta kTimeout = base::Seconds(30);
}  // namespace

@interface MockPageContextWrapper : PageContextWrapper
@property(nonatomic, assign) BOOL populateCalled;
@property(nonatomic, assign) BOOL lastUseRichExtraction;
@property(nonatomic, assign) BOOL lastUseRefactoredExtractor;
@property(nonatomic, assign) BOOL lastGraftCrossOriginFrameContent;
@property(nonatomic, assign) BOOL lastExtractPaidContent;
@end

@implementation MockPageContextWrapper

- (instancetype)initWithWebState:(web::WebState*)webState
                          config:(PageContextWrapperConfig)config
              completionCallback:
                  (base::OnceCallback<void(PageContextWrapperCallbackResponse)>)
                      completionCallback {
  self = [super initWithWebState:webState
                          config:config
              completionCallback:std::move(completionCallback)];
  if (self) {
    _lastUseRichExtraction = config.use_rich_extraction();
    _lastUseRefactoredExtractor = config.use_refactored_extractor();
    _lastGraftCrossOriginFrameContent =
        config.graft_cross_origin_frame_content();
    _lastExtractPaidContent = config.extract_paid_content();
  }
  return self;
}

- (void)populatePageContextFieldsAsync {
  self.populateCalled = YES;
}
- (void)populatePageContextFieldsAsyncWithTimeout:(base::TimeDelta)timeout {
  self.populateCalled = YES;
}
@end

// Test fixture for PageContextUtil.
class PageContextUtilTest : public PlatformTest {
 public:
  PageContextUtilTest() : web_state_(std::make_unique<web::FakeWebState>()) {}
  std::unique_ptr<web::FakeWebState> web_state_;
};

TEST_F(PageContextUtilTest, PopulateWhenNotLoading) {
  id mockWrapperClass = OCMClassMock([PageContextWrapper class]);
  MockPageContextWrapper* fakeWrapper = [[MockPageContextWrapper alloc]
        initWithWebState:web_state_.get()
                  config:PageContextWrapperConfigBuilder().Build()
      completionCallback:base::DoNothing()];
  OCMStub([mockWrapperClass alloc]).andReturn(fakeWrapper);

  web_state_->SetLoading(false);
  PopulatePageContext(fakeWrapper, web_state_.get());

  EXPECT_TRUE(fakeWrapper.populateCalled);
}

TEST_F(PageContextUtilTest, PopulateWhenLoading) {
  id mockWrapperClass = OCMClassMock([PageContextWrapper class]);
  MockPageContextWrapper* fakeWrapper = [[MockPageContextWrapper alloc]
        initWithWebState:web_state_.get()
                  config:PageContextWrapperConfigBuilder().Build()
      completionCallback:base::DoNothing()];
  OCMStub([mockWrapperClass alloc]).andReturn(fakeWrapper);

  web_state_->SetLoading(true);
  PopulatePageContext(fakeWrapper, web_state_.get());

  EXPECT_FALSE(fakeWrapper.populateCalled);

  web_state_->OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  EXPECT_TRUE(fakeWrapper.populateCalled);
}

TEST_F(PageContextUtilTest, PopulateWhenNotLoadingWithTimeout) {
  id mockWrapperClass = OCMClassMock([PageContextWrapper class]);
  MockPageContextWrapper* fakeWrapper = [[MockPageContextWrapper alloc]
        initWithWebState:web_state_.get()
                  config:PageContextWrapperConfigBuilder().Build()
      completionCallback:base::DoNothing()];
  OCMStub([mockWrapperClass alloc]).andReturn(fakeWrapper);

  web_state_->SetLoading(false);
  PopulatePageContextWithTimeout(fakeWrapper, web_state_.get(), kTimeout);

  EXPECT_TRUE(fakeWrapper.populateCalled);
}

TEST_F(PageContextUtilTest, PopulateWhenLoadingWithTimeout) {
  id mockWrapperClass = OCMClassMock([PageContextWrapper class]);
  MockPageContextWrapper* fakeWrapper = [[MockPageContextWrapper alloc]
        initWithWebState:web_state_.get()
                  config:PageContextWrapperConfigBuilder().Build()
      completionCallback:base::DoNothing()];
  OCMStub([mockWrapperClass alloc]).andReturn(fakeWrapper);

  web_state_->SetLoading(true);
  PopulatePageContextWithTimeout(fakeWrapper, web_state_.get(), kTimeout);

  web_state_->OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);

  EXPECT_TRUE(fakeWrapper.populateCalled);
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

TEST_F(PageContextUtilTest, CreatePageContextWrapperWithRichExtraction) {
  id mockWrapperClass = OCMClassMock([PageContextWrapper class]);
  MockPageContextWrapper* mockWrapper = [MockPageContextWrapper alloc];
  OCMStub([mockWrapperClass alloc]).andReturn(mockWrapper);

  CreatePageContextWrapper(web_state_.get(), true, base::DoNothing());

  EXPECT_TRUE(mockWrapper.lastUseRichExtraction);
  EXPECT_TRUE(mockWrapper.lastUseRefactoredExtractor);
  EXPECT_TRUE(mockWrapper.lastGraftCrossOriginFrameContent);
  EXPECT_TRUE(mockWrapper.lastExtractPaidContent);
}

TEST_F(PageContextUtilTest, CreatePageContextWrapperWithoutRichExtraction) {
  id mockWrapperClass = OCMClassMock([PageContextWrapper class]);
  MockPageContextWrapper* mockWrapper = [MockPageContextWrapper alloc];
  OCMStub([mockWrapperClass alloc]).andReturn(mockWrapper);

  CreatePageContextWrapper(web_state_.get(), false, base::DoNothing());

  EXPECT_FALSE(mockWrapper.lastUseRichExtraction);
}
