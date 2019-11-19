// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#include "base/base64.h"
#include "base/bind.h"
#include "base/macros.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/public/test/web_js_test.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/web_state.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
        base::TimeDelta::FromMilliseconds(kImageDelayInMs));
    result->set_content_type("image/png");
    result->set_content(image_binary);
    result->AddCustomHeader("Access-Control-Allow-Origin", "*");
    return std::move(result);
  }
  return nullptr;
}

// The ID param for calling JS, and will be regained in the message sent back.
const int kCallJavaScriptId = 66666;
// The command prefix for receiving messages from JS.
const std::string kCommandPrefix = "imageFetch";
}

// Test fixture for image_fetch.js testing.
class ImageFetchJsTest : public web::WebJsTest<web::WebTestWithWebState> {
 protected:
  ImageFetchJsTest()
      : web::WebJsTest<web::WebTestWithWebState>(@[ @"image_fetch" ]) {}

  void SetUp() override {
    WebTestWithWebState::SetUp();
    server_.RegisterDefaultHandler(base::BindRepeating(HandleRequest));
    ASSERT_TRUE(server_.Start());
    subscription_ = web_state()->AddScriptCommandCallback(
        base::BindRepeating(&ImageFetchJsTest::OnMessageFromJavaScript,
                            base::Unretained(this)),
        kCommandPrefix);
  }

  void OnMessageFromJavaScript(const base::DictionaryValue& message,
                               const GURL& page_url,
                               bool user_is_interacting,
                               web::WebFrame* sender_frame) {
    message_received_ = true;
    message_ = message.Clone();
  }

  // Subscription for JS message.
  std::unique_ptr<web::WebState::ScriptCommandSubscription> subscription_;

  net::EmbeddedTestServer server_;
  base::Value message_;
  bool message_received_ = false;

  DISALLOW_COPY_AND_ASSIGN(ImageFetchJsTest);
};

// Tests that __gCrWeb.imageFetch.getImageData works when the image is
// same-domain.
TEST_F(ImageFetchJsTest, TestGetSameDomainImageData) {
  const GURL image_url = server_.GetURL("/image");
  const GURL page_url = server_.GetURL("/");
  LoadHtmlAndInject([NSString stringWithFormat:@"<html><img src='%s'></html>",
                                               image_url.spec().c_str()],
                    page_url);

  ExecuteJavaScriptWithFormat(@"__gCrWeb.imageFetch.getImageData(%d, '%s');",
                              kCallJavaScriptId, image_url.spec().c_str());

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return message_received_;
  }));

  ASSERT_TRUE(message_.is_dict());
  const base::Value* id_key = message_.FindKey("id");
  ASSERT_TRUE(id_key);
  ASSERT_TRUE(id_key->is_double());
  EXPECT_EQ(kCallJavaScriptId, static_cast<int>(id_key->GetDouble()));
  // The fetched image data may not equal to original base64 data because of
  // recompression in JavaScript. Therefore decode the returned image data and
  // check if it can be rendered to an image.
  const base::Value* data = message_.FindKey("data");
  ASSERT_TRUE(data);
  ASSERT_TRUE(data->is_string());
  ASSERT_FALSE(data->GetString().empty());
  std::string decoded_data;
  ASSERT_TRUE(base::Base64Decode(data->GetString(), &decoded_data));
  UIImage* image =
      [UIImage imageWithData:[NSData dataWithBytes:decoded_data.c_str()
                                            length:decoded_data.size()]];
  EXPECT_TRUE(image);
  const base::Value* from = message_.FindKey("from");
  ASSERT_TRUE(from);
  ASSERT_TRUE(from->is_string());
  EXPECT_EQ("canvas", from->GetString());
}

// Tests that __gCrWeb.imageFetch.getImageData works when the image is
// cross-domain.
TEST_F(ImageFetchJsTest, TestGetCrossDomainImageData) {
  const GURL image_url = server_.GetURL("/image");
  // WebTestWithWebState::LoadHtml uses an HTTPS url for webpage as default. Use
  // an HTTP url instead, because XMLHttpRequest with HTTP url sent from HTTPS
  // website is forbidden due to the CORS policy.
  const GURL page_url("http://chrooooome.com");
  LoadHtmlAndInject([NSString stringWithFormat:@"<html><img src='%s'></html>",
                                               image_url.spec().c_str()],
                    page_url);

  ExecuteJavaScriptWithFormat(@"__gCrWeb.imageFetch.getImageData(%d, '%s');",
                              kCallJavaScriptId, image_url.spec().c_str());

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return message_received_;
  }));

  ASSERT_TRUE(message_.is_dict());
  const base::Value* id_key = message_.FindKey("id");
  ASSERT_TRUE(id_key);
  ASSERT_TRUE(id_key->is_double());
  EXPECT_EQ(kCallJavaScriptId, static_cast<int>(id_key->GetDouble()));
  const base::Value* data = message_.FindKey("data");
  ASSERT_TRUE(data);
  ASSERT_TRUE(data->is_string());
  EXPECT_EQ(kImageBase64, data->GetString());
  const base::Value* from = message_.FindKey("from");
  ASSERT_TRUE(from);
  ASSERT_TRUE(from->is_string());
  EXPECT_EQ("xhr", from->GetString());
}

// Tests that __gCrWeb.imageFetch.getImageData fails for timeout when the image
// response is delayed. In this test the image must be cross-domain, otherwise
// image data will be fetched from <img> by <canvas> directly.
TEST_F(ImageFetchJsTest, TestGetDelayedImageData) {
  const GURL image_url = server_.GetURL("/image_delayed");
  const GURL page_url("http://chrooooome.com");
  LoadHtmlAndInject([NSString stringWithFormat:@"<html><img src='%s'></html>",
                                               image_url.spec().c_str()],
                    page_url);

  ExecuteJavaScriptWithFormat(@"__gCrWeb.imageFetch.getImageData(%d, '%s');",
                              kCallJavaScriptId, image_url.spec().c_str());

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return message_received_;
  }));

  ASSERT_TRUE(message_.is_dict());
  const base::Value* id_key = message_.FindKey("id");
  ASSERT_TRUE(id_key);
  ASSERT_TRUE(id_key->is_double());
  EXPECT_EQ(kCallJavaScriptId, static_cast<int>(id_key->GetDouble()));
  EXPECT_FALSE(message_.FindKey("data"));
  EXPECT_FALSE(message_.FindKey("from"));
}
