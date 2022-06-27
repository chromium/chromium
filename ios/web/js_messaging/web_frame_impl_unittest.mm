// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/js_messaging/web_frame_impl.h"

#import <WebKit/WebKit.h>

#import "base/base64.h"
#include "base/bind.h"
#include "base/ios/ios_util.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#include "base/test/ios/wait_util.h"
#include "base/values.h"
#include "crypto/aead.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#include "ios/web/public/test/web_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using crypto::SymmetricKey;

namespace {
const char kFrameId[] = "1effd8f52a067c8d3a01762d3c41dfd8";

// A base64 encoded sample key.
const char kFrameKey[] = "R7lsXtR74c6R9A9k691gUQ8JAd0be+w//Lntgcbjwrc=";

// Returns a key which can be used to create a WebFrame.
std::unique_ptr<SymmetricKey> CreateKey() {
  std::string decoded_frame_key_string;
  base::Base64Decode(kFrameKey, &decoded_frame_key_string);
  return crypto::SymmetricKey::Import(crypto::SymmetricKey::Algorithm::AES,
                                      decoded_frame_key_string);
}

}  // namespace

namespace web {

typedef web::WebTest WebFrameImplTest;

// Tests creation of a WebFrame for the main frame without an encryption key.
TEST_F(WebFrameImplTest, CreateWebFrameForMainFrame) {
  FakeWebState fake_web_state;
  GURL security_origin;
  WebFrameImpl web_frame([[WKFrameInfo alloc] init], kFrameId,
                         /*is_main_frame=*/true, security_origin,
                         &fake_web_state);

  EXPECT_EQ(&fake_web_state, web_frame.GetWebState());
  EXPECT_TRUE(web_frame.IsMainFrame());
  EXPECT_TRUE(web_frame.CanCallJavaScriptFunction());
  EXPECT_EQ(security_origin, web_frame.GetSecurityOrigin());
  EXPECT_EQ(kFrameId, web_frame.GetFrameId());
}

// Tests creation of a WebFrame for the main frame with an encryption key.
TEST_F(WebFrameImplTest, CreateWebFrameForMainFrameWithKey) {
  FakeWebState fake_web_state;
  GURL security_origin;
  WebFrameImpl web_frame([[WKFrameInfo alloc] init], kFrameId,
                         /*is_main_frame=*/true, security_origin,
                         &fake_web_state);
  web_frame.SetEncryptionKey(CreateKey());

  EXPECT_EQ(&fake_web_state, web_frame.GetWebState());
  EXPECT_TRUE(web_frame.IsMainFrame());
  EXPECT_TRUE(web_frame.CanCallJavaScriptFunction());
  EXPECT_EQ(security_origin, web_frame.GetSecurityOrigin());
  EXPECT_EQ(kFrameId, web_frame.GetFrameId());
}

// Tests creation of a WebFrame for a frame which is not the main frame with an
// encryption key.
TEST_F(WebFrameImplTest, CreateWebFrameForIFrameWithKey) {
  FakeWebState fake_web_state;
  GURL security_origin;
  WebFrameImpl web_frame([[WKFrameInfo alloc] init], kFrameId,
                         /*is_main_frame=*/false, security_origin,
                         &fake_web_state);
  web_frame.SetEncryptionKey(CreateKey());

  EXPECT_EQ(&fake_web_state, web_frame.GetWebState());
  EXPECT_FALSE(web_frame.IsMainFrame());
  EXPECT_TRUE(web_frame.CanCallJavaScriptFunction());
  EXPECT_EQ(security_origin, web_frame.GetSecurityOrigin());
  EXPECT_EQ(kFrameId, web_frame.GetFrameId());
}

// Tests that the WebFrame properly creates JavaScript for the main frame when
// there is no encryption key.
TEST_F(WebFrameImplTest, CallJavaScriptFunctionMainFrameWithoutKey) {
  FakeWebState fake_web_state;
  fake_web_state.SetBrowserState(GetBrowserState());
  GURL security_origin;
  WebFrameImpl web_frame([[WKFrameInfo alloc] init], kFrameId,
                         /*is_main_frame=*/true, security_origin,
                         &fake_web_state);

  std::vector<base::Value> function_params;

  EXPECT_TRUE(
      web_frame.CallJavaScriptFunction("functionName", function_params));
  NSString* last_script =
      base::SysUTF16ToNSString(fake_web_state.GetLastExecutedJavascript());
  EXPECT_NSEQ(@"__gCrWeb.functionName()", last_script);

  function_params.push_back(base::Value("param1"));
  EXPECT_TRUE(
      web_frame.CallJavaScriptFunction("functionName", function_params));
  last_script =
      base::SysUTF16ToNSString(fake_web_state.GetLastExecutedJavascript());
  EXPECT_NSEQ(@"__gCrWeb.functionName(\"param1\")", last_script);

  function_params.push_back(base::Value(true));
  function_params.push_back(base::Value(27));
  function_params.push_back(base::Value(3.14));
  EXPECT_TRUE(
      web_frame.CallJavaScriptFunction("functionName", function_params));
  last_script =
      base::SysUTF16ToNSString(fake_web_state.GetLastExecutedJavascript());
  EXPECT_NSEQ(@"__gCrWeb.functionName(\"param1\",true,27,3.14)", last_script);
}

// Tests that the WebFrame does not create JavaScript for an iframe when there
// is no encryption key.
TEST_F(WebFrameImplTest, CallJavaScriptFunctionIFrameFrameWithoutKey) {
  FakeWebState fake_web_state;
  fake_web_state.SetBrowserState(GetBrowserState());
  GURL security_origin;
  WebFrameImpl web_frame([[WKFrameInfo alloc] init], kFrameId,
                         /*is_main_frame=*/false, security_origin,
                         &fake_web_state);

  std::vector<base::Value> function_params;
  function_params.push_back(base::Value("plaintextParam"));
  EXPECT_FALSE(
      web_frame.CallJavaScriptFunction("functionName", function_params));

  NSString* last_script =
      base::SysUTF16ToNSString(fake_web_state.GetLastExecutedJavascript());
  EXPECT_EQ(last_script.length, 0ul);
}

// Tests that the WebFrame can execute arbitrary JavaScript
// if and only if it is a main frame.
TEST_F(WebFrameImplTest, ExecuteJavaScript) {
  FakeWebState fake_web_state;
  fake_web_state.SetBrowserState(GetBrowserState());
  GURL security_origin;

  NSString* script = @"__gCrWeb = {};"
                     @"__gCrWeb['fakeFunction'] = function() {"
                     @"  return '10';"
                     @"}";

  WebFrameImpl web_frame([[WKFrameInfo alloc] init], kFrameId,
                         /*is_main_frame=*/true, security_origin,
                         &fake_web_state);

  EXPECT_TRUE(web_frame.ExecuteJavaScript(base::SysNSStringToUTF16(script)));

  WebFrameImpl web_frame2([[WKFrameInfo alloc] init], kFrameId,
                          /*is_main_frame=*/false, security_origin,
                          &fake_web_state);
  // Executing arbitrary code on an iframe should return false.
  EXPECT_FALSE(web_frame2.ExecuteJavaScript(base::SysNSStringToUTF16(script)));
}

// Tests that the WebFrame can execute arbitrary JavaScript given
// a callback if and only if it is a main frame.
TEST_F(WebFrameImplTest, ExecuteJavaScriptWithCallback) {
  FakeWebState fake_web_state;
  fake_web_state.SetBrowserState(GetBrowserState());

  GURL security_origin;

  NSString* script = @"__gCrWeb = {};"
                     @"__gCrWeb['fakeFunction'] = function() {"
                     @"  return '10';"
                     @"}";

  WebFrameImpl web_frame([[WKFrameInfo alloc] init], kFrameId,
                         /*is_main_frame=*/true, security_origin,
                         &fake_web_state);

  EXPECT_TRUE(
      web_frame.ExecuteJavaScript(base::SysNSStringToUTF16(script),
                                  base::BindOnce(^(const base::Value* value){
                                  })));

  WebFrameImpl web_frame2([[WKFrameInfo alloc] init], kFrameId,
                          /*is_main_frame=*/false, security_origin,
                          &fake_web_state);

  EXPECT_FALSE(
      web_frame2.ExecuteJavaScript(base::SysNSStringToUTF16(script),
                                   base::BindOnce(^(const base::Value* value){
                                   })));
}

}  // namespace web
