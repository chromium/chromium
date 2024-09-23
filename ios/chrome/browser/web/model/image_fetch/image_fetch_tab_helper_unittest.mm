// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/image_fetch/image_fetch_tab_helper.h"

#import <WebKit/WebKit.h>

#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/time/time.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/web/model/image_fetch/image_fetch_java_script_feature.h"
#import "ios/web/js_messaging/java_script_feature_manager.h"
#import "ios/web/public/js_messaging/java_script_feature.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/test/js_test_util_internal.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "net/http/http_util.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#import "services/network/public/mojom/url_response_head.mojom.h"
#import "services/network/test/test_url_loader_factory.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using base::test::ios::WaitUntilConditionOrTimeout;

namespace {
// Timeout for calling on ImageFetchTabHelper::GetImageData.
constexpr base::TimeDelta kWaitForGetImageDataTimeout = base::Seconds(1);

const char kImageUrl[] = "http://www.chrooooooooooome.com/";
const char kImageData[] = "abc";
}

// Test fixture for ImageFetchTabHelper class.
class ImageFetchTabHelperTest : public PlatformTest {
 public:
  ImageFetchTabHelperTest(const ImageFetchTabHelperTest&) = delete;
  ImageFetchTabHelperTest& operator=(const ImageFetchTabHelperTest&) = delete;

 protected:
  ImageFetchTabHelperTest()
      : web_client_(std::make_unique<web::FakeWebClient>()) {
    profile_ = TestProfileIOS::Builder().Build();

    web::WebState::CreateParams params(profile_.get());
    web_state_ = web::WebState::Create(params);
  }

  void SetUp() override {
    PlatformTest::SetUp();
    SetUpTestSharedURLLoaderFactory();
    GetWebClient()->SetJavaScriptFeatures(
        {ImageFetchJavaScriptFeature::GetInstance()});

    web::test::LoadHtml(@"<html></html>", web_state());
    ImageFetchTabHelper::CreateForWebState(web_state());
  }

  web::FakeWebClient* GetWebClient() {
    return static_cast<web::FakeWebClient*>(web_client_.Get());
  }

  // Sets up the network::TestURLLoaderFactory to handle download request.
  void SetUpTestSharedURLLoaderFactory() {
    profile_->SetSharedURLLoaderFactory(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_));

    network::mojom::URLResponseHeadPtr head =
        network::mojom::URLResponseHead::New();
    std::string raw_header = "HTTP/1.1 200 OK\n"
                             "Content-type: image/png\n\n";
    head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(raw_header));
    head->mime_type = "image/png";
    network::URLLoaderCompletionStatus status;
    status.decoded_body_length = strlen(kImageData);
    test_url_loader_factory_.AddResponse(GURL(kImageUrl), std::move(head),
                                         kImageData, status);
  }

  ImageFetchTabHelper* image_fetch_tab_helper() {
    return ImageFetchTabHelper::FromWebState(web_state());
  }

  // Returns the expected image data in NSData*.
  NSData* GetExpectedData() const {
    return [base::SysUTF8ToNSString(kImageData)
        dataUsingEncoding:NSUTF8StringEncoding];
  }

  web::WebState* web_state() { return web_state_.get(); }

  web::ScopedTestingWebClient web_client_;
  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::MainThreadType::IO};
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::WebState> web_state_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  base::HistogramTester histogram_tester_;
};

// Tests that ImageFetchTabHelper::GetImageData can get image data from Js.
TEST_F(ImageFetchTabHelperTest, GetImageDataWithJsSucceedFromCanvas) {
  // Inject fake `__gCrWeb.imageFetch.getImageData` that returns `kImageData`
  // in base64 format.
  id script_result = web::test::ExecuteJavaScriptForFeature(
      web_state(),
      [NSString
          stringWithFormat:
              @"__gCrWeb.imageFetch = {}; __gCrWeb.imageFetch.getImageData = "
               "function(id, url) { "
               "__gCrWeb.common.sendWebKitMessage('ImageFetchMessageHandler', "
               "{'id': id, 'data': btoa('%s'), 'from':'canvas'}); }; true;",
              kImageData],
      ImageFetchJavaScriptFeature::GetInstance());
  ASSERT_NSEQ(@YES, script_result);

  __block bool callback_invoked = false;
  image_fetch_tab_helper()->GetImageData(GURL(kImageUrl), web::Referrer(),
                                         ^(NSData* data) {
                                           ASSERT_TRUE(data);
                                           EXPECT_NSEQ(GetExpectedData(), data);
                                           callback_invoked = true;
                                         });

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForGetImageDataTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return callback_invoked;
  }));
  histogram_tester_.ExpectUniqueSample(
      kUmaGetImageDataByJsResult,
      ContextMenuGetImageDataByJsResult::kCanvasSucceed, 1);
}

// Tests that ImageFetchTabHelper::GetImageData can get image data from Js.
TEST_F(ImageFetchTabHelperTest, GetImageDataWithJsSucceedFromXmlHttpRequest) {
  // Inject fake `__gCrWeb.imageFetch.getImageData` that returns `kImageData`
  // in base64 format.
  id script_result = web::test::ExecuteJavaScriptForFeature(
      web_state(),
      [NSString
          stringWithFormat:
              @"__gCrWeb.imageFetch = {}; __gCrWeb.imageFetch.getImageData = "
               "function(id, url) { "
               "__gCrWeb.common.sendWebKitMessage('ImageFetchMessageHandler', "
               "{'id': id, 'data': btoa('%s'), 'from':'xhr'}); }; true;",
              kImageData],
      ImageFetchJavaScriptFeature::GetInstance());
  ASSERT_NSEQ(@YES, script_result);

  __block bool callback_invoked = false;
  image_fetch_tab_helper()->GetImageData(GURL(kImageUrl), web::Referrer(),
                                         ^(NSData* data) {
                                           ASSERT_TRUE(data);
                                           EXPECT_NSEQ(GetExpectedData(), data);
                                           callback_invoked = true;
                                         });

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForGetImageDataTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return callback_invoked;
  }));
  histogram_tester_.ExpectUniqueSample(
      kUmaGetImageDataByJsResult,
      ContextMenuGetImageDataByJsResult::kXMLHttpRequestSucceed, 1);
}

// Tests that ImageFetchTabHelper::GetImageData gets image data from server when
// Js fails.
TEST_F(ImageFetchTabHelperTest, GetImageDataWithJsFail) {
  id script_result = web::test::ExecuteJavaScriptForFeature(
      web_state(),
      @"__gCrWeb.imageFetch = {}; __gCrWeb.imageFetch.getImageData = "
       "function(id, url) { "
       "__gCrWeb.common.sendWebKitMessage('ImageFetchMessageHandler', "
       "{'id': id}); }; true;",
      ImageFetchJavaScriptFeature::GetInstance());
  ASSERT_NSEQ(@YES, script_result);

  __block bool callback_invoked = false;
  image_fetch_tab_helper()->GetImageData(GURL(kImageUrl), web::Referrer(),
                                         ^(NSData* data) {
                                           ASSERT_TRUE(data);
                                           EXPECT_NSEQ(GetExpectedData(), data);
                                           callback_invoked = true;
                                         });

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForGetImageDataTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return callback_invoked;
  }));
  histogram_tester_.ExpectUniqueSample(
      kUmaGetImageDataByJsResult, ContextMenuGetImageDataByJsResult::kFail, 1);
}

// Tests that ImageFetchTabHelper::GetImageData gets image data from server when
// Js does not send a message back.
TEST_F(ImageFetchTabHelperTest, GetImageDataWithJsTimeout) {
  // Inject fake `__gCrWeb.imageFetch.getImageData` that does not do anything.
  id script_result = web::test::ExecuteJavaScriptForFeature(
      web_state(),
      @"__gCrWeb.imageFetch = {}; __gCrWeb.imageFetch.getImageData = "
      @"function(id, url) {}; true;",
      ImageFetchJavaScriptFeature::GetInstance());
  ASSERT_NSEQ(@YES, script_result);

  __block bool callback_invoked = false;
  image_fetch_tab_helper()->GetImageData(GURL(kImageUrl), web::Referrer(),
                                         ^(NSData* data) {
                                           ASSERT_TRUE(data);
                                           EXPECT_NSEQ(GetExpectedData(), data);
                                           callback_invoked = true;
                                         });

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForGetImageDataTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return callback_invoked;
  }));
  histogram_tester_.ExpectUniqueSample(
      kUmaGetImageDataByJsResult, ContextMenuGetImageDataByJsResult::kTimeout,
      1);
}

// Tests that ImageFetchTabHelper::GetImageData gets image data from server when
// WebState is destroyed.
TEST_F(ImageFetchTabHelperTest, GetImageDataWithWebStateDestroy) {
  // Inject fake `__gCrWeb.imageFetch.getImageData` that does not do anything.
  id script_result = web::test::ExecuteJavaScriptForFeature(
      web_state(),
      @"__gCrWeb.imageFetch = {}; __gCrWeb.imageFetch.getImageData = "
      @"function(id, url) {}; true;",
      ImageFetchJavaScriptFeature::GetInstance());
  ASSERT_NSEQ(@YES, script_result);

  __block bool callback_invoked = false;
  image_fetch_tab_helper()->GetImageData(GURL(kImageUrl), web::Referrer(),
                                         ^(NSData* data) {
                                           ASSERT_TRUE(data);
                                           EXPECT_NSEQ(GetExpectedData(), data);
                                           callback_invoked = true;
                                         });

  web_state_.reset();

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForGetImageDataTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return callback_invoked;
  }));
  histogram_tester_.ExpectTotalCount(kUmaGetImageDataByJsResult, 0);
}

// Tests that ImageFetchTabHelper::GetImageData gets image data from server when
// WebState navigates to a new web page.
TEST_F(ImageFetchTabHelperTest, GetImageDataWithWebStateNavigate) {
  // Inject fake `__gCrWeb.imageFetch.getImageData` that does not do anything.
  id script_result = web::test::ExecuteJavaScriptForFeature(
      web_state(),
      @"__gCrWeb.imageFetch = {}; __gCrWeb.imageFetch.getImageData = "
      @"function(id, url) {}; true;",
      ImageFetchJavaScriptFeature::GetInstance());
  ASSERT_NSEQ(@YES, script_result);

  __block bool callback_invoked = false;
  image_fetch_tab_helper()->GetImageData(GURL(kImageUrl), web::Referrer(),
                                         ^(NSData* data) {
                                           ASSERT_TRUE(data);
                                           EXPECT_NSEQ(GetExpectedData(), data);
                                           callback_invoked = true;
                                         });

  web::test::LoadHtml(@"<html>new</html>", web_state()),
      GURL("http://new.webpage.com/");

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForGetImageDataTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return callback_invoked;
  }));
  histogram_tester_.ExpectTotalCount(kUmaGetImageDataByJsResult, 0);
}
