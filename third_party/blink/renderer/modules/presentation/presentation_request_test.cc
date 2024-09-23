// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/presentation/presentation_request.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_presentation_source.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_presentationsource_usvstring.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/exception_state_matchers.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {
namespace {

Member<V8UnionPresentationSourceOrUSVString> CreatePresentationSource(
    const String& url) {
  PresentationSource* source = PresentationSource::Create();
  source->setType(V8PresentationSourceType::Enum::kUrl);
  source->setUrl(url);
  return MakeGarbageCollected<V8UnionPresentationSourceOrUSVString>(source);
}

Member<V8UnionPresentationSourceOrUSVString> CreateMirroringSource() {
  PresentationSource* source = PresentationSource::Create();
  source->setType(V8PresentationSourceType::Enum::kMirroring);
  source->setAudioPlayback(V8AudioPlaybackDestination::Enum::kReceiver);
  source->setLatencyHint(V8CaptureLatency::Enum::kDefault);
  return MakeGarbageCollected<V8UnionPresentationSourceOrUSVString>(source);
}

HeapVector<Member<V8UnionPresentationSourceOrUSVString>> CreateUrlSources(
    const WTF::Vector<String>& urls) {
  HeapVector<Member<V8UnionPresentationSourceOrUSVString>> sources;
  for (const String& url : urls) {
    sources.push_back(
        MakeGarbageCollected<V8UnionPresentationSourceOrUSVString>(url));
  }
  return sources;
}

TEST(PresentationRequestTest, TestSingleUrlConstructor) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  PresentationRequest* request = PresentationRequest::Create(
      scope.GetExecutionContext(), "https://example.com",
      scope.GetExceptionState());
  ASSERT_FALSE(scope.GetExceptionState().HadException());

  WTF::Vector<KURL> request_urls = request->Urls();
  EXPECT_EQ(static_cast<size_t>(1), request_urls.size());
  EXPECT_TRUE(request_urls[0].IsValid());
  EXPECT_EQ("https://example.com/", request_urls[0].GetString());
}

TEST(PresentationRequestTest, TestMultipleUrlConstructor) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  HeapVector<Member<V8UnionPresentationSourceOrUSVString>> sources =
      CreateUrlSources({"https://example.com", "cast://deadbeef?param=foo"});

  PresentationRequest* request = PresentationRequest::Create(
      scope.GetExecutionContext(), sources, scope.GetExceptionState());
  ASSERT_FALSE(scope.GetExceptionState().HadException());

  WTF::Vector<KURL> request_urls = request->Urls();
  EXPECT_EQ(static_cast<size_t>(2), request_urls.size());
  EXPECT_TRUE(request_urls[0].IsValid());
  EXPECT_EQ("https://example.com/", request_urls[0].GetString());
  EXPECT_TRUE(request_urls[1].IsValid());
  EXPECT_EQ("cast://deadbeef?param=foo", request_urls[1].GetString());
}

TEST(PresentationRequestTest, TestMultipleUrlConstructorInvalidUrl) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  HeapVector<Member<V8UnionPresentationSourceOrUSVString>> sources =
      CreateUrlSources({"https://example.com", ""});

  PresentationRequest::Create(scope.GetExecutionContext(), sources,
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

  WTF::Vector<KURL> request_urls = request->Urls();
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

  HeapVector<Member<V8UnionPresentationSourceOrUSVString>> sources =
      CreateUrlSources({"http://example.com", "https://example1.com"});

  PresentationRequest::Create(scope.GetExecutionContext(), sources,
                              scope.GetExceptionState());
  EXPECT_THAT(scope.GetExceptionState(),
              HadException(DOMExceptionCode::kSecurityError));
}

TEST(PresentationRequestTest, TestMultipleUrlConstructorEmptySequence) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  HeapVector<Member<V8UnionPresentationSourceOrUSVString>> sources;

  PresentationRequest::Create(scope.GetExecutionContext(), sources,
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
  HeapVector<Member<V8UnionPresentationSourceOrUSVString>> sources =
      CreateUrlSources({"foobar:unknown", "https://example.com",
                        "cast://deadbeef?param=foo", "deadbeef:random"});

  PresentationRequest* request = PresentationRequest::Create(
      scope.GetExecutionContext(), sources, scope.GetExceptionState());
  ASSERT_THAT(scope.GetExceptionState(), HadNoException());

  WTF::Vector<KURL> request_urls = request->Urls();
  EXPECT_EQ(static_cast<size_t>(2), request_urls.size());
  EXPECT_TRUE(request_urls[0].IsValid());
  EXPECT_EQ("https://example.com/", request_urls[0].GetString());
  EXPECT_TRUE(request_urls[1].IsValid());
  EXPECT_EQ("cast://deadbeef?param=foo", request_urls[1].GetString());
}

TEST(PresentationRequestTest, TestMultipleUrlConstructorAllUnknownSchemes) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  HeapVector<Member<V8UnionPresentationSourceOrUSVString>> sources =
      CreateUrlSources({"foobar:unknown", "deadbeef:random"});

  PresentationRequest::Create(scope.GetExecutionContext(), sources,
                              scope.GetExceptionState());
  EXPECT_THAT(scope.GetExceptionState(),
              HadException(DOMExceptionCode::kNotSupportedError));
}

// If the site-initiated mirroring feature is disabled, then we do not allow
// the PresentationSource specialization of V8UnionPresentationSourceOrUSVString
// to be used to create a PresentationRequest.
TEST(PresentationRequestTest, TestPresentationSourceNotAllowed) {
  test::TaskEnvironment task_environment;
  ScopedSiteInitiatedMirroringForTest site_initiated_mirroring_enabled{false};
  V8TestingScope scope;
  PresentationRequest::Create(scope.GetExecutionContext(),
                              {CreatePresentationSource("https://example.com")},
                              scope.GetExceptionState());
  EXPECT_THAT(scope.GetExceptionState(),
              HadException(DOMExceptionCode::kNotSupportedError));
}

TEST(PresentationRequestTest, TestPresentationSourcesInConstructor) {
  test::TaskEnvironment task_environment;
  ScopedSiteInitiatedMirroringForTest site_initiated_mirroring_enabled{true};
  V8TestingScope scope;
  PresentationRequest* request = PresentationRequest::Create(
      scope.GetExecutionContext(),
      {CreatePresentationSource("https://example.com"),
       CreateMirroringSource()},
      scope.GetExceptionState());
  CHECK(request);
  ASSERT_THAT(scope.GetExceptionState(), HadNoException());
  EXPECT_EQ(static_cast<size_t>(2), request->Urls().size());
  EXPECT_TRUE(request->Urls()[0].IsValid());
  EXPECT_EQ("https://example.com/", request->Urls()[0].GetString());
  EXPECT_TRUE(request->Urls()[1].IsValid());
  // TODO(crbug.com/1267372): This makes a lot of assumptions about the
  // hardcoded URL in presentation_request.cc that should be removed.
  EXPECT_EQ(
      "cast:0F5096E8?streamingCaptureAudio=1&streamingTargetPlayoutDelayMillis="
      "400",
      request->Urls()[1].GetString());
}

TEST(PresentationRequestTest, TestInvalidPresentationSource) {
  test::TaskEnvironment task_environment;
  ScopedSiteInitiatedMirroringForTest site_initiated_mirroring_enabled{true};
  V8TestingScope scope;
  PresentationRequest::Create(scope.GetExecutionContext(),
                              {CreatePresentationSource("invalid_url")},
                              scope.GetExceptionState());
  EXPECT_THAT(scope.GetExceptionState(),
              HadException(DOMExceptionCode::kNotSupportedError));
}

}  // anonymous namespace
}  // namespace blink
