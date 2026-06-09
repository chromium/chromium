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
#import "ios/web/public/js_messaging/java_script_feature_util.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
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
}  // namespace

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

  web::WebFrame* main_frame() {
    web::WebFrame* frame = ImageFetchJavaScriptFeature::GetInstance()
                               ->GetWebFramesManager(web_state())
                               ->GetMainWebFrame();
    EXPECT_TRUE(frame);
    return frame;
  }

  std::string main_frame_id() { return main_frame()->GetFrameId(); }

  url::Origin main_frame_origin() { return main_frame()->GetSecurityOrigin(); }

  web::ScopedTestingWebClient web_client_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::WebState> web_state_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  base::HistogramTester histogram_tester_;
};

// Tests that ImageFetchTabHelper::GetImageData can get image data from Js.
TEST_F(ImageFetchTabHelperTest, GetImageDataWithJsSucceedFromCanvas) {
  // Inject fake `__gCrWeb.imageFetch.getImageData` that returns `kImageData`
  // in base64 format.
  id script_result = web::test::ExecuteJavaScriptForFeatureAndReturnResult(
      web_state(),
      [NSString
          stringWithFormat:
              @"const imageFetchApi = "
              @"__gCrWeb.getRegisteredApi('imageFetch');"
              @"function getImageData(id, url) { "
               "window.webkit.messageHandlers['ImageFetchMessageHandler']."
               " postMessage({'id': id, 'data': btoa('%s'), 'from':'canvas'});"
               "};"
               "imageFetchApi.addFunction('getImageData', getImageData); true;",
              kImageData],
      ImageFetchJavaScriptFeature::GetInstance());
  ASSERT_NSEQ(@YES, script_result);

  base::RunLoop run_loop;
  __block base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  image_fetch_tab_helper()->GetImageData(GURL(kImageUrl), web::Referrer(),
                                         main_frame_id(), main_frame_origin(),
                                         ^(NSData* data) {
                                           ASSERT_TRUE(data);
                                           EXPECT_NSEQ(GetExpectedData(), data);
                                           quit_closure.Run();
                                         });

  run_loop.Run();
  histogram_tester_.ExpectUniqueSample(
      kUmaGetImageDataByJsResult,
      ContextMenuGetImageDataByJsResult::kCanvasSucceed, 1);
}

// Tests that ImageFetchTabHelper::GetImageData can get image data from Js.
TEST_F(ImageFetchTabHelperTest, GetImageDataWithJsSucceedFromXmlHttpRequest) {
  // Inject fake `__gCrWeb.imageFetch.getImageData` that returns `kImageData`
  // in base64 format.
  id script_result = web::test::ExecuteJavaScriptForFeatureAndReturnResult(
      web_state(),
      [NSString
          stringWithFormat:
              @"const imageFetchApi = "
              @"__gCrWeb.getRegisteredApi('imageFetch');"
               "function getImageData(id, url) { "
               "window.webkit.messageHandlers['ImageFetchMessageHandler']."
               "postMessage({'id': id, 'data': btoa('%s'), 'from':'xhr'});"
               "}; imageFetchApi.addFunction('getImageData', getImageData); "
               "true;",
              kImageData],
      ImageFetchJavaScriptFeature::GetInstance());
  ASSERT_NSEQ(@YES, script_result);
  base::RunLoop run_loop;
  __block base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  image_fetch_tab_helper()->GetImageData(GURL(kImageUrl), web::Referrer(),
                                         main_frame_id(), main_frame_origin(),
                                         ^(NSData* data) {
                                           ASSERT_TRUE(data);
                                           EXPECT_NSEQ(GetExpectedData(), data);
                                           quit_closure.Run();
                                         });

  run_loop.Run();
  histogram_tester_.ExpectUniqueSample(
      kUmaGetImageDataByJsResult,
      ContextMenuGetImageDataByJsResult::kXMLHttpRequestSucceed, 1);
}

// Tests that ImageFetchTabHelper::GetImageData gets image data from server when
// Js fails.
TEST_F(ImageFetchTabHelperTest, GetImageDataWithJsFail) {
  id script_result = web::test::ExecuteJavaScriptForFeatureAndReturnResult(
      web_state(),
      @"const imageFetchApi = "
      @"__gCrWeb.getRegisteredApi('imageFetch');"
       "function getImageData(id, url) { "
       "  "
       "window.webkit.messageHandlers['ImageFetchMessageHandler'].postMessage({"
       "'id': id}); }; "
       "imageFetchApi.addFunction('getImageData', getImageData); true;",
      ImageFetchJavaScriptFeature::GetInstance());
  ASSERT_NSEQ(@YES, script_result);

  base::RunLoop run_loop;
  __block base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  image_fetch_tab_helper()->GetImageData(GURL(kImageUrl), web::Referrer(),
                                         main_frame_id(), main_frame_origin(),
                                         ^(NSData* data) {
                                           ASSERT_TRUE(data);
                                           EXPECT_NSEQ(GetExpectedData(), data);
                                           quit_closure.Run();
                                         });

  run_loop.Run();
  histogram_tester_.ExpectUniqueSample(
      kUmaGetImageDataByJsResult, ContextMenuGetImageDataByJsResult::kFail, 1);
}

// Tests that ImageFetchTabHelper::GetImageData gets image data from server when
// Js does not send a message back.
TEST_F(ImageFetchTabHelperTest, GetImageDataWithJsTimeout) {
  // Inject fake `__gCrWeb.imageFetch.getImageData` that does not do anything.
  id script_result = web::test::ExecuteJavaScriptForFeatureAndReturnResult(
      web_state(),
      @"const imageFetchApi = "
      @"__gCrWeb.getRegisteredApi('imageFetch');"
      @"function getImageData(id, url) {};"
      @"imageFetchApi.addFunction('getImageData', getImageData); true;",
      ImageFetchJavaScriptFeature::GetInstance());
  ASSERT_NSEQ(@YES, script_result);

  base::RunLoop run_loop;
  __block base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  image_fetch_tab_helper()->GetImageData(GURL(kImageUrl), web::Referrer(),
                                         main_frame_id(), main_frame_origin(),
                                         ^(NSData* data) {
                                           ASSERT_TRUE(data);
                                           EXPECT_NSEQ(GetExpectedData(), data);
                                           quit_closure.Run();
                                         });

  run_loop.Run();
  histogram_tester_.ExpectUniqueSample(
      kUmaGetImageDataByJsResult, ContextMenuGetImageDataByJsResult::kTimeout,
      1);
}

// Tests that ImageFetchTabHelper::GetImageData gets image data from server when
// WebState is destroyed.
TEST_F(ImageFetchTabHelperTest, GetImageDataWithWebStateDestroy) {
  // Inject fake `__gCrWeb.imageFetch.getImageData` that does not do anything.
  id script_result = web::test::ExecuteJavaScriptForFeatureAndReturnResult(
      web_state(),
      @"const imageFetchApi = "
      @"__gCrWeb.getRegisteredApi('imageFetch');"
      @"function getImageData(id, url) {};"
      @"imageFetchApi.addFunction('getImageData', getImageData); true;",
      ImageFetchJavaScriptFeature::GetInstance());
  ASSERT_NSEQ(@YES, script_result);

  base::RunLoop run_loop;
  __block base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  image_fetch_tab_helper()->GetImageData(GURL(kImageUrl), web::Referrer(),
                                         main_frame_id(), main_frame_origin(),
                                         ^(NSData* data) {
                                           ASSERT_TRUE(data);
                                           EXPECT_NSEQ(GetExpectedData(), data);
                                           quit_closure.Run();
                                         });

  web_state_.reset();

  run_loop.Run();
  histogram_tester_.ExpectTotalCount(kUmaGetImageDataByJsResult, 0);
}

// Tests that ImageFetchTabHelper::GetImageData gets image data from server when
// WebState navigates to a new web page.
TEST_F(ImageFetchTabHelperTest, GetImageDataWithWebStateNavigate) {
  // Inject fake `__gCrWeb.imageFetch.getImageData` that does not do anything.
  id script_result = web::test::ExecuteJavaScriptForFeatureAndReturnResult(
      web_state(),
      @"const imageFetchApi = "
      @"__gCrWeb.getRegisteredApi('imageFetch');"
      @"function getImageData(id, url) {};"
      @"imageFetchApi.addFunction('getImageData', getImageData);true;",
      ImageFetchJavaScriptFeature::GetInstance());
  ASSERT_NSEQ(@YES, script_result);

  base::RunLoop run_loop;
  __block base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  image_fetch_tab_helper()->GetImageData(GURL(kImageUrl), web::Referrer(),
                                         main_frame_id(), main_frame_origin(),
                                         ^(NSData* data) {
                                           ASSERT_TRUE(data);
                                           EXPECT_NSEQ(GetExpectedData(), data);
                                           quit_closure.Run();
                                         });

  web::test::LoadHtml(@"<html>new</html>", web_state()),
      GURL("http://new.webpage.com/");

  run_loop.Run();
  histogram_tester_.ExpectTotalCount(kUmaGetImageDataByJsResult, 0);
}

// Tests that ImageFetchTabHelper::GetImageData falls back to using ImageFetcher
// when the origin is mismatched (wrong domain).
TEST_F(ImageFetchTabHelperTest,
       GetImageDataWithMismatchedOriginFallsbackToImageFetcher) {
  url::Origin mismatched_origin =
      url::Origin::Create(GURL("https://mismatch.origin/"));
  base::RunLoop run_loop;
  __block base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  image_fetch_tab_helper()->GetImageData(GURL(kImageUrl), web::Referrer(),
                                         main_frame_id(), mismatched_origin,
                                         ^(NSData* data) {
                                           ASSERT_TRUE(data);
                                           EXPECT_NSEQ(GetExpectedData(), data);
                                           quit_closure.Run();
                                         });

  run_loop.Run();
  histogram_tester_.ExpectTotalCount(kUmaGetImageDataByJsResult, 0);
}

// Tests that ImageFetchTabHelper::GetImageData falls back to using ImageFetcher
// when the frame is null.
TEST_F(ImageFetchTabHelperTest,
       GetImageDataWithNullFrameFallsbackToImageFetcher) {
  base::RunLoop run_loop;
  __block base::RepeatingClosure quit_closure = run_loop.QuitClosure();
  image_fetch_tab_helper()->GetImageData(GURL(kImageUrl), web::Referrer(),
                                         "invalid-frame-id",
                                         main_frame_origin(), ^(NSData* data) {
                                           ASSERT_TRUE(data);
                                           EXPECT_NSEQ(GetExpectedData(), data);
                                           quit_closure.Run();
                                         });

  run_loop.Run();
  histogram_tester_.ExpectTotalCount(kUmaGetImageDataByJsResult, 0);
}

// Tests that ImageFetchTabHelper::GetImageData can successfully get image data
// from a subframe (iframe).
TEST_F(ImageFetchTabHelperTest, GetImageDataFromSubframeSucceeds) {
  NSString* html =
      @"<html>"
       "  <iframe id='iframe' srcdoc='<html><body><img "
       "src=\"http://www.chrooooooooooome.com/\"></body></html>'></iframe>"
       "</html>";
  web::test::LoadHtml(html, web_state());

  web::WebFramesManager* frames_manager =
      ImageFetchJavaScriptFeature::GetInstance()->GetWebFramesManager(
          web_state());

  __block web::WebFrame* subframe = nullptr;
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForGetImageDataTimeout, ^{
    for (web::WebFrame* frame : frames_manager->GetAllWebFrames()) {
      if (!frame->IsMainFrame()) {
        subframe = frame;
        return YES;
      }
    }
    return NO;
  }));
  ASSERT_TRUE(subframe);

  NSString* inject_script = [NSString
      stringWithFormat:
          @"const imageFetchApi = "
          @"__gCrWeb.getRegisteredApi('imageFetch');"
          @"function getImageData(id, url) { "
           "window.webkit.messageHandlers['ImageFetchMessageHandler']."
           " postMessage({'id': id, 'data': btoa('%s'), 'from':'canvas'});"
           "};"
           "imageFetchApi.addFunction('getImageData', getImageData); true;",
          kImageData];

  __block bool js_executed = false;
  subframe->ExecuteJavaScript(base::SysNSStringToUTF16(inject_script),
                              base::BindOnce(^(const base::Value* result) {
                                js_executed = true;
                              }));

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForGetImageDataTimeout, ^{
    return js_executed;
  }));

  base::RunLoop run_loop2;
  __block base::RepeatingClosure quit_closure2 = run_loop2.QuitClosure();
  image_fetch_tab_helper()->GetImageData(
      GURL(kImageUrl), web::Referrer(), subframe->GetFrameId(),
      subframe->GetSecurityOrigin(), ^(NSData* data) {
        ASSERT_TRUE(data);
        EXPECT_NSEQ(GetExpectedData(), data);
        quit_closure2.Run();
      });

  run_loop2.Run();
  histogram_tester_.ExpectUniqueSample(
      kUmaGetImageDataByJsResult,
      ContextMenuGetImageDataByJsResult::kCanvasSucceed, 1);
}
