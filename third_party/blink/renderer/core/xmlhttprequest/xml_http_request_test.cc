// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/xmlhttprequest/xml_http_request.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

class XMLHttpRequestTest : public PageTestBase {
 protected:
};

// An XHR with an origin with `CanLoadLocalResources` set cannot set forbidden
// request headers. It was historically allowed, and this is a regression test.
// See https://crbug.com/567527 for details.
TEST_F(XMLHttpRequestTest, ForbiddenRequestHeaderWithLocalOrigin) {
  GetFrame().DomWindow()->GetMutableSecurityOrigin()->GrantLoadLocalResources();

  auto* xhr = XMLHttpRequest::Create(ToScriptStateForMainWorld(&GetFrame()));

  xhr->open(http_names::kGET, "https://example.com/", ASSERT_NO_EXCEPTION);
  xhr->setRequestHeader(AtomicString("host"), AtomicString("example.com"),
                        ASSERT_NO_EXCEPTION);
  EXPECT_FALSE(xhr->HasRequestHeaderForTesting(AtomicString("host")));
}

class XMLHttpRequestLoadIgnoreLimitsTest
    : public PageTestBase,
      public testing::WithParamInterface<bool> {
 protected:
  XMLHttpRequestLoadIgnoreLimitsTest() {
    feature_list_.InitWithFeatureState(
        features::kRequiresLoadIgnoreLimitsForDomWindowsOnly, GetParam());
  }

  bool IsRequiresLoadIgnoreLimitsForDomWindowsOnlyEnabled() const {
    return GetParam();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         XMLHttpRequestLoadIgnoreLimitsTest,
                         testing::Bool());

TEST_P(XMLHttpRequestLoadIgnoreLimitsTest, DomWindowContext) {
  // Async XHR in a DOMWindow context should not set LOAD_IGNORE_LIMITS on the
  // TCP request and bypass socket pool size limitations.
  const KURL async_url("data:text/plain,async");
  auto* async_xhr =
      XMLHttpRequest::Create(ToScriptStateForMainWorld(&GetFrame()));
  async_xhr->open(AtomicString("GET"), async_url, /*async=*/true,
                  ASSERT_NO_EXCEPTION);
  async_xhr->send(
      static_cast<V8UnionDocumentOrXMLHttpRequestBodyInit*>(nullptr),
      ASSERT_NO_EXCEPTION);
  Resource* async_resource = GetDocument().Fetcher()->CachedResource(async_url);
  ASSERT_TRUE(async_resource);
  EXPECT_FALSE(async_resource->GetResourceRequest().RequiresLoadIgnoreLimits());

  // Sync XHR in a DOMWindow context should always set LOAD_IGNORE_LIMITS,
  // regardless of kRequiresLoadIgnoreLimitsForDomWindowsOnly.
  const KURL sync_url("data:text/plain,sync");
  auto* sync_xhr =
      XMLHttpRequest::Create(ToScriptStateForMainWorld(&GetFrame()));
  sync_xhr->open(AtomicString("GET"), sync_url, /*async=*/false,
                 ASSERT_NO_EXCEPTION);
  sync_xhr->send(static_cast<V8UnionDocumentOrXMLHttpRequestBodyInit*>(nullptr),
                 ASSERT_NO_EXCEPTION);
  Resource* sync_resource = GetDocument().Fetcher()->CachedResource(sync_url);
  ASSERT_TRUE(sync_resource);
  EXPECT_TRUE(sync_resource->GetResourceRequest().RequiresLoadIgnoreLimits());
}

namespace {

class CountingEventListener final : public NativeEventListener {
 public:
  void Invoke(ExecutionContext*, Event*) override { ++count_; }
  int Count() const { return count_; }

 private:
  int count_ = 0;
};

class ReopenOnHeadersReceivedListener final : public NativeEventListener {
 public:
  ReopenOnHeadersReceivedListener(XMLHttpRequest* xhr,
                                  const KURL& next_url,
                                  bool send_immediately = false)
      : xhr_(xhr), next_url_(next_url), send_immediately_(send_immediately) {}

  void Invoke(ExecutionContext*, Event*) override {
    if (!reopened_ && xhr_->readyState() == XMLHttpRequest::kHeadersReceived) {
      reopened_ = true;
      xhr_->open(http_names::kGET, next_url_, /*async=*/true,
                 ASSERT_NO_EXCEPTION);
      if (send_immediately_) {
        xhr_->send(
            static_cast<V8UnionDocumentOrXMLHttpRequestBodyInit*>(nullptr),
            ASSERT_NO_EXCEPTION);
      }
    }
  }

  bool Reopened() const { return reopened_; }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(xhr_);
    NativeEventListener::Trace(visitor);
  }

 private:
  Member<XMLHttpRequest> xhr_;
  KURL next_url_;
  bool send_immediately_;
  bool reopened_ = false;
};

}  // namespace

class XMLHttpRequestSimTest : public SimTest {};

TEST_F(XMLHttpRequestSimTest, OpenDuringHeadersReceivedDoesNotAppendChunk) {
  SimRequest main_resource("https://example.com/index.html", "text/html");
  LoadURL("https://example.com/index.html");
  main_resource.Complete("<!DOCTYPE html>");

  SimSubresourceRequest first_request("https://example.com/first.txt",
                                      "text/plain");
  SimSubresourceRequest second_request("https://example.com/second.txt",
                                       "text/plain");

  auto* xhr = XMLHttpRequest::Create(
      ToScriptStateForMainWorld(GetDocument().GetFrame()));
  auto* listener = MakeGarbageCollected<ReopenOnHeadersReceivedListener>(
      xhr, KURL("https://example.com/second.txt"));
  auto* progress_counter = MakeGarbageCollected<CountingEventListener>();
  xhr->addEventListener(event_type_names::kReadystatechange, listener);
  xhr->addEventListener(event_type_names::kProgress, progress_counter);

  xhr->open(http_names::kGET, "https://example.com/first.txt",
            ASSERT_NO_EXCEPTION);
  xhr->send(static_cast<V8UnionDocumentOrXMLHttpRequestBodyInit*>(nullptr),
            ASSERT_NO_EXCEPTION);
  test::RunPendingTasks();

  first_request.Start();
  first_request.Write("first_response");
  first_request.Finish();

  ASSERT_TRUE(listener->Reopened());
  EXPECT_EQ(XMLHttpRequest::kOpened, xhr->readyState());
  EXPECT_TRUE(xhr->responseText(ASSERT_NO_EXCEPTION).empty());
  EXPECT_EQ(0, progress_counter->Count());

  xhr->send(static_cast<V8UnionDocumentOrXMLHttpRequestBodyInit*>(nullptr),
            ASSERT_NO_EXCEPTION);
  test::RunPendingTasks();
  second_request.Complete("second_response");
  test::RunPendingTasks();

  EXPECT_EQ(XMLHttpRequest::kDone, xhr->readyState());
  EXPECT_EQ("second_response", xhr->responseText(ASSERT_NO_EXCEPTION));
}

TEST_F(XMLHttpRequestSimTest, OpenAndSendDuringEmptyBodyFinishLoading) {
  SimRequest main_resource("https://example.com/index.html", "text/html");
  LoadURL("https://example.com/index.html");
  main_resource.Complete("<!DOCTYPE html>");

  SimSubresourceRequest first_request("https://example.com/empty.txt",
                                      "text/plain");
  SimSubresourceRequest second_request("https://example.com/second.txt",
                                       "text/plain");

  auto* xhr = XMLHttpRequest::Create(
      ToScriptStateForMainWorld(GetDocument().GetFrame()));
  auto* listener = MakeGarbageCollected<ReopenOnHeadersReceivedListener>(
      xhr, KURL("https://example.com/second.txt"), /*send_immediately=*/true);
  xhr->addEventListener(event_type_names::kReadystatechange, listener);

  xhr->open(http_names::kGET, "https://example.com/empty.txt",
            ASSERT_NO_EXCEPTION);
  xhr->send(static_cast<V8UnionDocumentOrXMLHttpRequestBodyInit*>(nullptr),
            ASSERT_NO_EXCEPTION);
  test::RunPendingTasks();

  first_request.Complete("");
  ASSERT_TRUE(listener->Reopened());
  EXPECT_EQ(XMLHttpRequest::kOpened, xhr->readyState());

  test::RunPendingTasks();
  second_request.Complete("second_response");
  test::RunPendingTasks();

  EXPECT_EQ(XMLHttpRequest::kDone, xhr->readyState());
  EXPECT_EQ("second_response", xhr->responseText(ASSERT_NO_EXCEPTION));
}

}  // namespace blink
