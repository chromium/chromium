// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/js_messaging/web_frame_impl.h"

#import "base/base64.h"
#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#include "base/test/ios/wait_util.h"
#include "base/values.h"
#include "crypto/aead.h"
#import "ios/web/public/test/fakes/test_web_state.h"
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

struct RouteMessageParameters {
  NSString* encoded_message_payload = nil;
  NSString* encoded_message_iv = nil;
  NSString* encoded_function_payload = nil;
  NSString* encoded_function_iv = nil;
  NSString* frame_id = nil;
};

RouteMessageParameters ParametersFromFunctionCallString(
    NSString* function_call) {
  NSRange parameters_start = [function_call rangeOfString:@"("];
  NSRange parameters_end = [function_call rangeOfString:@")"];
  NSMutableString* parameter_string = [[function_call
      substringWithRange:NSMakeRange(parameters_start.location + 1,
                                     parameters_end.location -
                                         parameters_start.location - 1)]
      mutableCopy];
  // Create array string and replace single quotes with double quotes in
  // preparation for JSON serialization.
  [parameter_string insertString:@"[" atIndex:0];
  [parameter_string appendString:@"]"];
  NSString* final_string =
      [parameter_string stringByReplacingOccurrencesOfString:@"'"
                                                  withString:@"\""];

  NSData* data = [final_string dataUsingEncoding:NSUTF8StringEncoding];
  NSError* error = nil;
  NSArray* jsonArray =
      [NSJSONSerialization JSONObjectWithData:data
                                      options:NSJSONReadingMutableContainers |
                                              NSJSONReadingMutableLeaves
                                        error:&error];

  RouteMessageParameters parsed_params;
  if (jsonArray.count == 3 && !error) {
    parsed_params.encoded_message_iv = jsonArray[0][@"iv"];
    parsed_params.encoded_message_payload = jsonArray[0][@"payload"];
    parsed_params.encoded_function_iv = jsonArray[1][@"iv"];
    parsed_params.encoded_function_payload = jsonArray[1][@"payload"];
    parsed_params.frame_id = jsonArray[2];
  }

  return parsed_params;
}

}  // namespace

namespace web {

typedef web::WebTest WebFrameImplTest;

// Tests creation of a WebFrame for the main frame without an encryption key.
TEST_F(WebFrameImplTest, CreateWebFrameForMainFrame) {
  TestWebState test_web_state;
  GURL security_origin;
  WebFrameImpl web_frame(kFrameId, /*is_main_frame=*/true, security_origin,
                         &test_web_state);

  EXPECT_EQ(&test_web_state, web_frame.GetWebState());
  EXPECT_TRUE(web_frame.IsMainFrame());
  EXPECT_TRUE(web_frame.CanCallJavaScriptFunction());
  EXPECT_EQ(security_origin, web_frame.GetSecurityOrigin());
  EXPECT_EQ(kFrameId, web_frame.GetFrameId());
}

// Tests creation of a WebFrame for the main frame with an encryption key.
TEST_F(WebFrameImplTest, CreateWebFrameForMainFrameWithKey) {
  TestWebState test_web_state;
  GURL security_origin;
  WebFrameImpl web_frame(kFrameId, /*is_main_frame=*/true, security_origin,
                         &test_web_state);
  web_frame.SetEncryptionKey(CreateKey());

  EXPECT_EQ(&test_web_state, web_frame.GetWebState());
  EXPECT_TRUE(web_frame.IsMainFrame());
  EXPECT_TRUE(web_frame.CanCallJavaScriptFunction());
  EXPECT_EQ(security_origin, web_frame.GetSecurityOrigin());
  EXPECT_EQ(kFrameId, web_frame.GetFrameId());
}

// Tests creation of a WebFrame for a frame which is not the main frame without
// an encryption key.
TEST_F(WebFrameImplTest, CreateWebFrameForIFrame) {
  TestWebState test_web_state;
  GURL security_origin;
  WebFrameImpl web_frame(kFrameId, /*is_main_frame=*/false, security_origin,
                         &test_web_state);

  EXPECT_EQ(&test_web_state, web_frame.GetWebState());
  EXPECT_FALSE(web_frame.IsMainFrame());
  EXPECT_FALSE(web_frame.CanCallJavaScriptFunction());
  EXPECT_EQ(security_origin, web_frame.GetSecurityOrigin());
  EXPECT_EQ(kFrameId, web_frame.GetFrameId());
}

// Tests creation of a WebFrame for a frame which is not the main frame with an
// encryption key.
TEST_F(WebFrameImplTest, CreateWebFrameForIFrameWithKey) {
  TestWebState test_web_state;
  GURL security_origin;
  WebFrameImpl web_frame(kFrameId, /*is_main_frame=*/false, security_origin,
                         &test_web_state);
  web_frame.SetEncryptionKey(CreateKey());

  EXPECT_EQ(&test_web_state, web_frame.GetWebState());
  EXPECT_FALSE(web_frame.IsMainFrame());
  EXPECT_TRUE(web_frame.CanCallJavaScriptFunction());
  EXPECT_EQ(security_origin, web_frame.GetSecurityOrigin());
  EXPECT_EQ(kFrameId, web_frame.GetFrameId());
}

// Tests that |CallJavaScriptFunction| encrypts the message and passes it to
// __gCrWeb.message.routeMessage in the main frame.
TEST_F(WebFrameImplTest, CallJavaScriptFunction) {
  TestWebState test_web_state;
  GURL security_origin;
  WebFrameImpl web_frame(kFrameId, /*is_main_frame=*/false, security_origin,
                         &test_web_state);
  web_frame.SetEncryptionKey(CreateKey());

  std::vector<base::Value> function_params;
  function_params.push_back(base::Value("plaintextParam"));
  EXPECT_TRUE(
      web_frame.CallJavaScriptFunction("functionName", function_params));

  NSString* last_script =
      base::SysUTF16ToNSString(test_web_state.GetLastExecutedJavascript());
  EXPECT_TRUE([last_script hasPrefix:@"__gCrWeb.message.routeMessage"]);
  // Verify the message does not contain the plaintext function name or
  // parameters.
  EXPECT_FALSE([last_script containsString:@"functionName"]);
  EXPECT_FALSE([last_script containsString:@"plaintextParam"]);

  RouteMessageParameters params = ParametersFromFunctionCallString(last_script);

  // Verify that the message and function payload are properly base64 encoded
  // strings.
  std::string decoded_function_payload;
  EXPECT_TRUE(base::Base64Decode(
      base::SysNSStringToUTF8(params.encoded_function_payload),
      &decoded_function_payload));
  std::string decoded_message_payload;
  EXPECT_TRUE(base::Base64Decode(
      base::SysNSStringToUTF8(params.encoded_message_payload),
      &decoded_message_payload));
  // Verify the function does not contain the plaintext function name or
  // parameters.
  EXPECT_FALSE([base::SysUTF8ToNSString(decoded_function_payload)
      containsString:@"functionName"]);
  EXPECT_FALSE([base::SysUTF8ToNSString(decoded_function_payload)
      containsString:@"plaintextParam"]);

  // Verify that the initialization vector is a properly base64 encoded string
  // for both payloads.
  std::string function_iv_string =
      base::SysNSStringToUTF8(params.encoded_function_iv);
  std::string decoded_function_iv;
  EXPECT_TRUE(base::Base64Decode(function_iv_string, &decoded_function_iv));
  std::string message_iv_string =
      base::SysNSStringToUTF8(params.encoded_message_iv);
  std::string decoded_message_iv;
  EXPECT_TRUE(base::Base64Decode(message_iv_string, &decoded_message_iv));

  // Ensure the frame ID matches.
  EXPECT_NSEQ(base::SysUTF8ToNSString(kFrameId), params.frame_id);
}

// Tests that the WebFrame uses different initialization vectors for two
// sequential calls to |CallJavaScriptFunction|.
TEST_F(WebFrameImplTest, CallJavaScriptFunctionUniqueInitializationVector) {
  TestWebState test_web_state;
  GURL security_origin;
  WebFrameImpl web_frame(kFrameId, /*is_main_frame=*/false, security_origin,
                         &test_web_state);
  web_frame.SetEncryptionKey(CreateKey());

  std::vector<base::Value> function_params;
  function_params.push_back(base::Value("plaintextParam"));
  EXPECT_TRUE(
      web_frame.CallJavaScriptFunction("functionName", function_params));

  NSString* last_script1 =
      base::SysUTF16ToNSString(test_web_state.GetLastExecutedJavascript());
  RouteMessageParameters params1 =
      ParametersFromFunctionCallString(last_script1);

  // Call JavaScript Function again to verify that the same initialization
  // vector is not reused and that the ciphertext is different.
  EXPECT_TRUE(
      web_frame.CallJavaScriptFunction("functionName", function_params));
  NSString* last_script2 =
      base::SysUTF16ToNSString(test_web_state.GetLastExecutedJavascript());
  RouteMessageParameters params2 =
      ParametersFromFunctionCallString(last_script2);

  EXPECT_NSNE(params1.encoded_function_payload,
              params2.encoded_function_payload);
  EXPECT_NSNE(params1.encoded_function_iv, params2.encoded_function_iv);
}

// Tests that the WebFrame properly encodes and encrypts all parameters for
// |CallJavaScriptFunction|.
TEST_F(WebFrameImplTest, CallJavaScriptFunctionMessageProperlyEncoded) {
  std::unique_ptr<SymmetricKey> key = CreateKey();
  const std::string key_string = key->key();
  // Use an arbitrary nonzero message id to ensure it isn't matching a zero
  // value by chance.
  const int initial_message_id = 11;

  TestWebState test_web_state;
  GURL security_origin;
  WebFrameImpl web_frame(kFrameId, /*is_main_frame=*/false, security_origin,
                         &test_web_state);
  web_frame.SetEncryptionKey(std::move(key));
  web_frame.SetNextMessageId(initial_message_id);

  std::vector<base::Value> function_params;
  std::string plaintext_param("plaintextParam");
  function_params.push_back(base::Value(plaintext_param));
  EXPECT_TRUE(
      web_frame.CallJavaScriptFunction("functionName", function_params));

  NSString* last_script =
      base::SysUTF16ToNSString(test_web_state.GetLastExecutedJavascript());
  RouteMessageParameters params = ParametersFromFunctionCallString(last_script);

  std::string decoded_function_ciphertext;
  EXPECT_TRUE(base::Base64Decode(
      base::SysNSStringToUTF8(params.encoded_function_payload),
      &decoded_function_ciphertext));

  std::string decoded_function_iv;
  EXPECT_TRUE(
      base::Base64Decode(base::SysNSStringToUTF8(params.encoded_function_iv),
                         &decoded_function_iv));

  std::string decoded_message_ciphertext;
  EXPECT_TRUE(base::Base64Decode(
      base::SysNSStringToUTF8(params.encoded_message_payload),
      &decoded_message_ciphertext));

  std::string decoded_message_iv;
  EXPECT_TRUE(base::Base64Decode(
      base::SysNSStringToUTF8(params.encoded_message_iv), &decoded_message_iv));

  // Decrypt message
  crypto::Aead aead(crypto::Aead::AES_256_GCM);
  aead.Init(&key_string);
  std::string function_plaintext;
  EXPECT_TRUE(aead.Open(decoded_function_ciphertext, decoded_function_iv,
                        base::NumberToString(initial_message_id),
                        &function_plaintext));
  std::string message_plaintext;
  EXPECT_TRUE(aead.Open(decoded_message_ciphertext, decoded_message_iv,
                        /*additional_data=*/nullptr, &message_plaintext));

  base::Optional<base::Value> parsed_function_result =
      base::JSONReader::Read(function_plaintext, false);
  EXPECT_TRUE(parsed_function_result.has_value());
  ASSERT_TRUE(parsed_function_result.value().is_dict());

  const std::string* decrypted_function_name =
      parsed_function_result.value().FindStringKey("functionName");
  ASSERT_TRUE(decrypted_function_name);
  EXPECT_EQ("functionName", *decrypted_function_name);

  base::Value* decrypted_parameters =
      parsed_function_result.value().FindKeyOfType("parameters",
                                                   base::Value::Type::LIST);
  ASSERT_TRUE(decrypted_parameters);
  ASSERT_EQ(function_params.size(), decrypted_parameters->GetList().size());
  EXPECT_EQ(plaintext_param, decrypted_parameters->GetList()[0].GetString());

  base::Optional<base::Value> parsed_message_result =
      base::JSONReader::Read(message_plaintext, false);
  EXPECT_TRUE(parsed_message_result.has_value());
  ASSERT_TRUE(parsed_message_result.value().is_dict());

  base::Optional<int> decrypted_message_id =
      parsed_message_result.value().FindIntKey("messageId");
  ASSERT_TRUE(decrypted_message_id.has_value());
  EXPECT_EQ(decrypted_message_id.value(), initial_message_id);

  base::Optional<bool> decrypted_respond_with_result =
      parsed_message_result.value().FindBoolKey("replyWithResult");
  ASSERT_TRUE(decrypted_respond_with_result.has_value());
  EXPECT_FALSE(decrypted_respond_with_result.value());
}

// Tests that the WebFrame properly encodes and encrypts the respondWithResult
// value when |CallJavaScriptFunction| is called with a callback.
TEST_F(WebFrameImplTest, CallJavaScriptFunctionRespondWithResult) {
  std::unique_ptr<SymmetricKey> key = CreateKey();
  const std::string key_string = key->key();
  // Use an arbitrary nonzero message id to ensure it isn't matching a zero
  // value by chance.
  const int initial_message_id = 11;

  TestWebState test_web_state;
  GURL security_origin;
  WebFrameImpl web_frame(kFrameId, /*is_main_frame=*/false, security_origin,
                         &test_web_state);
  web_frame.SetEncryptionKey(std::move(key));
  web_frame.SetNextMessageId(initial_message_id);

  std::vector<base::Value> function_params;
  std::string plaintext_param("plaintextParam");
  function_params.push_back(base::Value(plaintext_param));
  EXPECT_TRUE(web_frame.CallJavaScriptFunction(
      "functionName", function_params,
      base::BindOnce(^(const base::Value* value){
      }),
      base::TimeDelta::FromSeconds(5)));

  NSString* last_script =
      base::SysUTF16ToNSString(test_web_state.GetLastExecutedJavascript());
  RouteMessageParameters params = ParametersFromFunctionCallString(last_script);

  std::string decoded_message_ciphertext;
  EXPECT_TRUE(base::Base64Decode(
      base::SysNSStringToUTF8(params.encoded_message_payload),
      &decoded_message_ciphertext));

  std::string decoded_message_iv;
  EXPECT_TRUE(base::Base64Decode(
      base::SysNSStringToUTF8(params.encoded_message_iv), &decoded_message_iv));

  // Decrypt message
  crypto::Aead aead(crypto::Aead::AES_256_GCM);
  aead.Init(&key_string);
  std::string message_plaintext;
  EXPECT_TRUE(aead.Open(decoded_message_ciphertext, decoded_message_iv,
                        /*additional_data=*/nullptr, &message_plaintext));

  base::Optional<base::Value> parsed_result =
      base::JSONReader::Read(message_plaintext, false);
  EXPECT_TRUE(parsed_result.has_value());
  ASSERT_TRUE(parsed_result.value().is_dict());

  base::Optional<bool> decrypted_respond_with_result =
      parsed_result.value().FindBoolKey("replyWithResult");
  ASSERT_TRUE(decrypted_respond_with_result.has_value());
  EXPECT_TRUE(decrypted_respond_with_result.value());
}

// Tests that the WebFrame properly creates JavaScript for the main frame when
// there is no encryption key.
TEST_F(WebFrameImplTest, CallJavaScriptFunctionMainFrameWithoutKey) {
  TestWebState test_web_state;
  GURL security_origin;
  WebFrameImpl web_frame(kFrameId, /*is_main_frame=*/true, security_origin,
                         &test_web_state);

  std::vector<base::Value> function_params;

  EXPECT_TRUE(
      web_frame.CallJavaScriptFunction("functionName", function_params));
  NSString* last_script =
      base::SysUTF16ToNSString(test_web_state.GetLastExecutedJavascript());
  EXPECT_NSEQ(@"__gCrWeb.functionName()", last_script);

  function_params.push_back(base::Value("param1"));
  EXPECT_TRUE(
      web_frame.CallJavaScriptFunction("functionName", function_params));
  last_script =
      base::SysUTF16ToNSString(test_web_state.GetLastExecutedJavascript());
  EXPECT_NSEQ(@"__gCrWeb.functionName(\"param1\")", last_script);

  function_params.push_back(base::Value(true));
  function_params.push_back(base::Value(27));
  function_params.push_back(base::Value(3.14));
  EXPECT_TRUE(
      web_frame.CallJavaScriptFunction("functionName", function_params));
  last_script =
      base::SysUTF16ToNSString(test_web_state.GetLastExecutedJavascript());
  EXPECT_NSEQ(@"__gCrWeb.functionName(\"param1\",true,27,3.14)", last_script);
}

// Tests that the WebFrame does not create JavaScript for an iframe when there
// is no encryption key.
TEST_F(WebFrameImplTest, CallJavaScriptFunctionIFrameFrameWithoutKey) {
  TestWebState test_web_state;
  GURL security_origin;
  WebFrameImpl web_frame(kFrameId, /*is_main_frame=*/false, security_origin,
                         &test_web_state);

  std::vector<base::Value> function_params;
  function_params.push_back(base::Value("plaintextParam"));
  EXPECT_FALSE(
      web_frame.CallJavaScriptFunction("functionName", function_params));

  NSString* last_script =
      base::SysUTF16ToNSString(test_web_state.GetLastExecutedJavascript());
  EXPECT_EQ(last_script.length, 0ul);
}

}  // namespace web
