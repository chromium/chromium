// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/image_fetch/image_fetch_java_script_feature.h"

#import <UIKit/UIKit.h>

#import "base/base64.h"
#import "base/functional/bind.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForJSCompletionTimeout;

namespace {
// Base64 data of a PNG image.
const char kImageBase64[] =
    "iVBORw0KGgoAAAANSUhEUgAAAIQAAAB+AQMAAADfgyubAAAABlBMVEX+/"
    "v4ODg5o52ONAAABfElEQVRIx+3UO27DMAwAUBoeNBQIe4DAvkLHFDDim/"
    "QMHp2lVpAhY49Ubb2GgFxARZcMqVjKn1SUgl6g4WBAz6QByRQB7vGf48Hy4yWWgUV5IQagvsRC"
    "LG0sRZAhljIInRMphCjSrLEgi5IysLy7SCrOQSFrlnqIpWHZhp1c45mloVh2LH0mOyFfLJ9hJ7"
    "EUJyGnVHily48boiPhlTpm8hZLqEAUchwFpFR1LOqmNFIU6ab1iWwy6YWskHQnhXNcluNEI4Qc"
    "K2SNtE9E0d6kOaWhWBpFByNaY8M5OhWVi2zDDgkzWQG5RAohFqmCVNYip7DoGykG/"
    "SaTXkip0XeZOCFK42WUx/"
    "lwQAF+"
    "2yBPk2wBoSYLLN0kHipAMmGn05eKIPUohV2kYdFAl3Jq1tJDD6FTyKOZryicJ6G5ERUVDmoPMB"
    "At1/"
    "hgx3EwLD+"
    "sJDIQpF0Ougzl7TmIW0aGB2h5UV9vXChv7TQF5tHjpvHzO5CQX72Gcncf3vf4M34AIPQSr4HM1"
    "a4AAAAASUVORK5CYII=";

// Delay of the http response for image.
const int kImageDelayInMs = 1000;

// Request handler of the http server.
std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
    const net::test_server::HttpRequest& request) {
  std::string image_binary;
  EXPECT_TRUE(base::Base64Decode(kImageBase64, &image_binary));

  if (request.GetURL().path() == "/image") {
    auto result = std::make_unique<net::test_server::BasicHttpResponse>();
    result->set_content_type("image/png");
    result->set_content(image_binary);
    result->AddCustomHeader("Access-Control-Allow-Origin", "*");
    return std::move(result);
  }
  if (request.GetURL().path() == "/image_delayed") {
    auto result = std::make_unique<net::test_server::DelayedHttpResponse>(
        base::Milliseconds(kImageDelayInMs));
    result->set_content_type("image/png");
    result->set_content(image_binary);
    result->AddCustomHeader("Access-Control-Allow-Origin", "*");
    return std::move(result);
  }
  return nullptr;
}

// The ID param for calling JS, and will be regained in the message sent back.
const int kCallJavaScriptId = 66666;
}

class ImageFetchJavaScriptFeatureTest
    : public PlatformTest,
      public ImageFetchJavaScriptFeature::Handler {
 protected:
  ImageFetchJavaScriptFeatureTest()
      : web_client_(std::make_unique<web::FakeWebClient>()),
        feature_(base::BindRepeating(
            &ImageFetchJavaScriptFeatureTest::GetHandlerFromTestFixture,
            base::Unretained(this))) {}

  void SetUp() override {
    PlatformTest::SetUp();

    profile_ = TestProfileIOS::Builder().Build();

    web::WebState::CreateParams params(profile_.get());
    web_state_ = web::WebState::Create(params);
    web_state_->GetView();
    web_state_->SetKeepRenderProcessAlive(true);

    GetWebClient()->SetJavaScriptFeatures({&feature_});

    server_.RegisterDefaultHandler(base::BindRepeating(HandleRequest));
    ASSERT_TRUE(server_.Start());
  }

  ImageFetchJavaScriptFeature::Handler* GetHandlerFromTestFixture(
      web::WebState*) {
    return this;
  }

  web::FakeWebClient* GetWebClient() {
    return static_cast<web::FakeWebClient*>(web_client_.Get());
  }

  void HandleJsSuccess(int call_id,
                       std::string& decoded_data,
                       std::string& from) override {
    EXPECT_FALSE(message_received_);
    message_received_ = true;
    message_id_ = call_id;
    message_from_ = from;
    message_decoded_data_ = decoded_data;
  }

  void HandleJsFailure(int call_id) override {
    EXPECT_FALSE(message_received_);
    message_received_ = true;
    message_id_ = call_id;
  }

  void WaitForResult() {
    ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
      return message_received_;
    }));
  }

  web::WebState* web_state() { return web_state_.get(); }

  web::ScopedTestingWebClient web_client_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::WebState> web_state_;

  ImageFetchJavaScriptFeature feature_;
  net::EmbeddedTestServer server_;
  bool message_received_ = false;
  int message_id_ = 0;
  std::string message_from_;
  std::string message_decoded_data_;
};

// Tests that __gCrWeb.imageFetch.getImageData works when the image is
// same-domain.
TEST_F(ImageFetchJavaScriptFeatureTest, TestGetSameDomainImageData) {
  const GURL image_url = server_.GetURL("/image");
  const GURL page_url = server_.GetURL("/");
  web::test::LoadHtml([NSString stringWithFormat:@"<html><img src='%s'></html>",
                                                 image_url.spec().c_str()],
                      page_url, web_state());

  feature_.GetImageData(web_state(), kCallJavaScriptId, image_url);
  WaitForResult();

  ASSERT_TRUE(message_received_);
  EXPECT_EQ(kCallJavaScriptId, message_id_);
  // The fetched image data may not equal to original base64 data because of
  // recompression in JavaScript. Therefore decode the returned image data and
  // check if it can be rendered to an image.
  UIImage* image = [UIImage
      imageWithData:[NSData dataWithBytes:message_decoded_data_.c_str()
                                   length:message_decoded_data_.size()]];
  EXPECT_TRUE(image);
  EXPECT_EQ("canvas", message_from_);
}

// Tests that __gCrWeb.imageFetch.getImageData works when the image is
// cross-domain.
TEST_F(ImageFetchJavaScriptFeatureTest, TestGetCrossDomainImageData) {
  const GURL image_url = server_.GetURL("/image");
  // web::test::LoadHtml uses an HTTPS url for webpage as default. Use
  // an HTTP url instead, because XMLHttpRequest with HTTP url sent from HTTPS
  // website is forbidden due to the CORS policy.
  const GURL page_url("http://chrooooome.com");
  web::test::LoadHtml([NSString stringWithFormat:@"<html><img src='%s'></html>",
                                                 image_url.spec().c_str()],
                      page_url, web_state());

  feature_.GetImageData(web_state(), kCallJavaScriptId, image_url);
  WaitForResult();

  ASSERT_TRUE(message_received_);
  EXPECT_EQ(kCallJavaScriptId, message_id_);

  std::string image_binary;
  ASSERT_TRUE(base::Base64Decode(kImageBase64, &image_binary));
  EXPECT_EQ(image_binary, message_decoded_data_);
  EXPECT_EQ("xhr", message_from_);
}

// Tests that __gCrWeb.imageFetch.getImageData fails for timeout when the image
// response is delayed. In this test the image must be cross-domain, otherwise
// image data will be fetched from <img> by <canvas> directly.
TEST_F(ImageFetchJavaScriptFeatureTest, TestGetDelayedImageData) {
  const GURL image_url = server_.GetURL("/image_delayed");
  const GURL page_url("http://chrooooome.com");
  web::test::LoadHtml([NSString stringWithFormat:@"<html><img src='%s'></html>",
                                                 image_url.spec().c_str()],
                      page_url, web_state());

  feature_.GetImageData(web_state(), kCallJavaScriptId, image_url);
  WaitForResult();

  ASSERT_TRUE(message_received_);
  EXPECT_EQ(kCallJavaScriptId, message_id_);
  EXPECT_TRUE(message_decoded_data_.empty());
  EXPECT_TRUE(message_from_.empty());
}
