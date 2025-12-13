// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/presentation/presentation_request.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/exception_state_matchers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {
namespace {

TEST(PresentationRequestTest, TestSingleUrlConstructor) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  PresentationRequest* request = PresentationRequest::Create(
      scope.GetExecutionContext(), "https://example.com",
      scope.GetExceptionState());
  ASSERT_FALSE(scope.GetExceptionState().HadException());

  Vector<KURL> request_urls = request->Urls();
  EXPECT_EQ(static_cast<size_t>(1), request_urls.size());
  EXPECT_TRUE(request_urls[0].IsValid());
  EXPECT_EQ("https://example.com/", request_urls[0].GetString());
}

TEST(PresentationRequestTest, TestMultipleUrlConstructor) {
  test::TaskEnvironment task_environment;
  Vector<String> urls;
  urls.push_back("https://example.com");
  urls.push_back("cast://deadbeef?param=foo");
  V8TestingScope scope;

  PresentationRequest* request = PresentationRequest::Create(
      scope.GetExecutionContext(), urls, scope.GetExceptionState());
  ASSERT_FALSE(scope.GetExceptionState().HadException());

  Vector<KURL> request_urls = request->Urls();
  EXPECT_EQ(static_cast<size_t>(2), request_urls.size());
  EXPECT_TRUE(request_urls[0].IsValid());
  EXPECT_EQ("https://example.com/", request_urls[0].GetString());
  EXPECT_TRUE(request_urls[1].IsValid());
  EXPECT_EQ("cast://deadbeef?param=foo", request_urls[1].GetString());
}

TEST(PresentationRequestTest, TestMultipleUrlConstructorInvalidUrl) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  Vector<String> urls;
  urls.push_back("https://example.com");
  urls.push_back("");

  PresentationRequest::Create(scope.GetExecutionContext(), urls,
                              scope.GetExceptionState());
  EXPECT_THAT(scope.GetExceptionState(),
              HadException(DOMExceptionCode::kSyntaxError));
}

TEST(PresentationRequestTest, TestMixedContentNotCheckedForNonHttpFamily) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope(KURL("https://example.test"));

  PresentationRequest* request = PresentationRequest::Create(
      scope.GetExecutionContext(), "cast://deadbeef?param=foo",
      scope.GetExceptionState());
  ASSERT_FALSE(scope.GetExceptionState().HadException());

  Vector<KURL> request_urls = request->Urls();
  EXPECT_EQ(static_cast<size_t>(1), request_urls.size());
  EXPECT_TRUE(request_urls[0].IsValid());
  EXPECT_EQ("cast://deadbeef?param=foo", request_urls[0].GetString());
}

TEST(PresentationRequestTest, TestSingleUrlConstructorMixedContent) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope(KURL("https://example.test"));

  PresentationRequest::Create(scope.GetExecutionContext(), "http://example.com",
                              scope.GetExceptionState());
  EXPECT_THAT(scope.GetExceptionState(),
              HadException(DOMExceptionCode::kSecurityError));
}

TEST(PresentationRequestTest, TestMultipleUrlConstructorMixedContent) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope(KURL("https://example.test"));

  Vector<String> urls;
  urls.push_back("http://example.com");
  urls.push_back("https://example1.com");

  PresentationRequest::Create(scope.GetExecutionContext(), urls,
                              scope.GetExceptionState());
  EXPECT_THAT(scope.GetExceptionState(),
              HadException(DOMExceptionCode::kSecurityError));
}

TEST(PresentationRequestTest, TestMultipleUrlConstructorEmptySequence) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  Vector<String> urls;

  PresentationRequest::Create(scope.GetExecutionContext(), urls,
                              scope.GetExceptionState());
  EXPECT_THAT(scope.GetExceptionState(),
              HadException(DOMExceptionCode::kNotSupportedError));
}

TEST(PresentationRequestTest, TestSingleUrlConstructorUnknownScheme) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  PresentationRequest::Create(scope.GetExecutionContext(), "foobar:unknown",
                              scope.GetExceptionState());
  EXPECT_THAT(scope.GetExceptionState(),
              HadException(DOMExceptionCode::kNotSupportedError));
}

TEST(PresentationRequestTest, TestMultipleUrlConstructorSomeUnknownSchemes) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  Vector<String> urls;
  urls.push_back("foobar:unknown");
  urls.push_back("https://example.com");
  urls.push_back("cast://deadbeef?param=foo");
  urls.push_back("deadbeef:random");

  PresentationRequest* request = PresentationRequest::Create(
      scope.GetExecutionContext(), urls, scope.GetExceptionState());
  ASSERT_THAT(scope.GetExceptionState(), HadNoException());

  Vector<KURL> request_urls = request->Urls();
  EXPECT_EQ(static_cast<size_t>(2), request_urls.size());
  EXPECT_TRUE(request_urls[0].IsValid());
  EXPECT_EQ("https://example.com/", request_urls[0].GetString());
  EXPECT_TRUE(request_urls[1].IsValid());
  EXPECT_EQ("cast://deadbeef?param=foo", request_urls[1].GetString());
}

TEST(PresentationRequestTest, TestMultipleUrlConstructorAllUnknownSchemes) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  Vector<String> urls;
  urls.push_back("foobar:unknown");
  urls.push_back("deadbeef:random");

  PresentationRequest::Create(scope.GetExecutionContext(), urls,
                              scope.GetExceptionState());
  EXPECT_THAT(scope.GetExceptionState(),
              HadException(DOMExceptionCode::kNotSupportedError));
}

}  // anonymous namespace
}  // namespace blink
