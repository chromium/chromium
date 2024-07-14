/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <functional>
#include <list>
#include <memory>

#include "base/memory/ptr_util.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/prerender/prerender.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/web_cache.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_no_state_prefetch_client.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html_element_type_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

namespace {

class TestWebNoStatePrefetchClient : public WebNoStatePrefetchClient {
 public:
  TestWebNoStatePrefetchClient() = default;
  virtual ~TestWebNoStatePrefetchClient() = default;

 private:
  bool IsPrefetchOnly() override { return false; }
};

class MockNoStatePrefetchProcessor
    : public mojom::blink::NoStatePrefetchProcessor {
 public:
  explicit MockNoStatePrefetchProcessor(
      mojo::PendingReceiver<mojom::blink::NoStatePrefetchProcessor>
          pending_receiver) {
    receiver_for_prefetch_.Bind(std::move(pending_receiver));
  }
  ~MockNoStatePrefetchProcessor() override = default;

  // mojom::blink::NoStatePrefetchProcessor implementation
  void Start(mojom::blink::PrerenderAttributesPtr attributes) override {
    attributes_ = std::move(attributes);
  }
  void Cancel() override { cancel_count_++; }

  // Returns the number of times |Cancel| was called.
  size_t CancelCount() const { return cancel_count_; }

  const KURL& Url() const { return attributes_->url; }
  mojom::blink::PrerenderTriggerType PrerenderTriggerType() const {
    return attributes_->trigger_type;
  }

 private:
  mojom::blink::PrerenderAttributesPtr attributes_;
  mojo::Receiver<mojom::blink::NoStatePrefetchProcessor> receiver_for_prefetch_{
      this};

  size_t cancel_count_ = 0;
};

class PrerenderTest : public testing::Test {
 public:
  ~PrerenderTest() override {
    if (web_view_helper_.GetWebView())
      UnregisterMockPrerenderProcessor();
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

  void Initialize(const char* base_url, const char* file_name) {
    // TODO(crbug.com/751425): We should use the mock functionality
    // via |web_view_helper_|.
    url_test_helpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8(base_url), test::CoreTestDataPath(),
        WebString::FromUTF8(file_name));
    web_view_helper_.Initialize();
    web_view_helper_.GetWebView()->SetNoStatePrefetchClient(
        &no_state_prefetch_client_);

    GetBrowserInterfaceBroker().SetBinderForTesting(
        mojom::blink::NoStatePrefetchProcessor::Name_,
        WTF::BindRepeating(&PrerenderTest::Bind, WTF::Unretained(this)));

    frame_test_helpers::LoadFrame(
        web_view_helper_.GetWebView()->MainFrameImpl(),
        std::string(base_url) + file_name);
  }

  void Bind(mojo::ScopedMessagePipeHandle message_pipe_handle) {
    auto processor = std::make_unique<MockNoStatePrefetchProcessor>(
        mojo::PendingReceiver<mojom::blink::NoStatePrefetchProcessor>(
            std::move(message_pipe_handle)));
    processors_.push_back(std::move(processor));
  }

  void NavigateAway() {
    frame_test_helpers::LoadFrame(
        web_view_helper_.GetWebView()->MainFrameImpl(), "about:blank");
    test::RunPendingTasks();
  }

  void Close() {
    UnregisterMockPrerenderProcessor();
    web_view_helper_.LocalMainFrame()->CollectGarbageForTesting();
    web_view_helper_.Reset();

    WebCache::Clear();

    test::RunPendingTasks();
  }

  void ExecuteScript(const char* code) {
    web_view_helper_.LocalMainFrame()->ExecuteScript(
        WebScriptSource(WebString::FromUTF8(code)));
    test::RunPendingTasks();
  }

  std::vector<std::unique_ptr<MockNoStatePrefetchProcessor>>& processors() {
    return processors_;
  }

  bool IsUseCounted(WebFeature feature) {
    return web_view_helper_.LocalMainFrame()
        ->GetFrame()
        ->GetDocument()
        ->IsUseCounted(feature);
  }

 private:
  void UnregisterMockPrerenderProcessor() {
    GetBrowserInterfaceBroker().SetBinderForTesting(
        mojom::blink::NoStatePrefetchProcessor::Name_, {});
  }

  const BrowserInterfaceBrokerProxy& GetBrowserInterfaceBroker() {
    return web_view_helper_.LocalMainFrame()
        ->GetFrame()
        ->GetBrowserInterfaceBroker();
  }
  test::TaskEnvironment task_environment_;

  std::vector<std::unique_ptr<MockNoStatePrefetchProcessor>> processors_;

  TestWebNoStatePrefetchClient no_state_prefetch_client_;

  frame_test_helpers::WebViewHelper web_view_helper_;
};

}  // namespace

TEST_F(PrerenderTest, SinglePrerender) {
  Initialize("http://example.com/", "prerender/single_prerender.html");
  ASSERT_EQ(processors().size(), 1u);
  MockNoStatePrefetchProcessor& processor = *processors()[0];

  EXPECT_EQ(KURL("http://example.com/prerender"), processor.Url());
  EXPECT_EQ(mojom::blink::PrerenderTriggerType::kLinkRelPrerender,
            processor.PrerenderTriggerType());

  EXPECT_EQ(0u, processor.CancelCount());
}

TEST_F(PrerenderTest, CancelPrerender) {
  Initialize("http://example.com/", "prerender/single_prerender.html");
  ASSERT_EQ(processors().size(), 1u);
  MockNoStatePrefetchProcessor& processor = *processors()[0];

  EXPECT_EQ(0u, processor.CancelCount());
  ExecuteScript("removePrerender()");
  EXPECT_EQ(1u, processor.CancelCount());
}

TEST_F(PrerenderTest, TwoPrerenders) {
  Initialize("http://example.com/", "prerender/multiple_prerenders.html");

  ASSERT_EQ(processors().size(), 2u);
  MockNoStatePrefetchProcessor& first_processor = *processors()[0];
  EXPECT_EQ(KURL("http://example.com/first"), first_processor.Url());
  MockNoStatePrefetchProcessor& second_processor = *processors()[1];
  EXPECT_EQ(KURL("http://example.com/second"), second_processor.Url());

  EXPECT_EQ(0u, first_processor.CancelCount());
  EXPECT_EQ(0u, second_processor.CancelCount());
}

TEST_F(PrerenderTest, TwoPrerendersRemovingFirstThenNavigating) {
  Initialize("http://example.com/", "prerender/multiple_prerenders.html");

  ASSERT_EQ(processors().size(), 2u);
  MockNoStatePrefetchProcessor& first_processor = *processors()[0];
  MockNoStatePrefetchProcessor& second_processor = *processors()[1];

  EXPECT_EQ(0u, first_processor.CancelCount());
  EXPECT_EQ(0u, second_processor.CancelCount());

  ExecuteScript("removeFirstPrerender()");

  EXPECT_EQ(1u, first_processor.CancelCount());
  EXPECT_EQ(0u, second_processor.CancelCount());

  NavigateAway();

  EXPECT_EQ(1u, first_processor.CancelCount());
  EXPECT_EQ(0u, second_processor.CancelCount());
}

TEST_F(PrerenderTest, TwoPrerendersAddingThird) {
  Initialize("http://example.com/", "prerender/multiple_prerenders.html");

  ASSERT_EQ(processors().size(), 2u);
  MockNoStatePrefetchProcessor& first_processor = *processors()[0];
  MockNoStatePrefetchProcessor& second_processor = *processors()[1];

  EXPECT_EQ(0u, first_processor.CancelCount());
  EXPECT_EQ(0u, second_processor.CancelCount());

  ExecuteScript("addThirdPrerender()");

  ASSERT_EQ(processors().size(), 3u);
  MockNoStatePrefetchProcessor& third_processor = *processors()[2];

  EXPECT_EQ(0u, first_processor.CancelCount());
  EXPECT_EQ(0u, second_processor.CancelCount());
  EXPECT_EQ(0u, third_processor.CancelCount());
}

TEST_F(PrerenderTest, MutateTarget) {
  Initialize("http://example.com/", "prerender/single_prerender.html");
  ASSERT_EQ(processors().size(), 1u);
  MockNoStatePrefetchProcessor& processor = *processors()[0];

  EXPECT_EQ(KURL("http://example.com/prerender"), processor.Url());

  EXPECT_EQ(0u, processor.CancelCount());

  // Change the href of this prerender, make sure this is treated as a remove
  // and add.
  ExecuteScript("mutateTarget()");

  ASSERT_EQ(processors().size(), 2u);
  MockNoStatePrefetchProcessor& mutated_processor = *processors()[1];
  EXPECT_EQ(KURL("http://example.com/mutated"), mutated_processor.Url());

  EXPECT_EQ(1u, processor.CancelCount());
  EXPECT_EQ(0u, mutated_processor.CancelCount());
}

TEST_F(PrerenderTest, MutateRel) {
  Initialize("http://example.com/", "prerender/single_prerender.html");
  ASSERT_EQ(processors().size(), 1u);
  MockNoStatePrefetchProcessor& processor = *processors()[0];

  EXPECT_EQ(KURL("http://example.com/prerender"), processor.Url());

  EXPECT_EQ(0u, processor.CancelCount());

  // Change the rel of this prerender, make sure this is treated as a remove.
  ExecuteScript("mutateRel()");

  EXPECT_EQ(1u, processor.CancelCount());
}

TEST_F(PrerenderTest, OriginTypeUseCounter) {
  Initialize("http://example.com/", "prerender/any_prerender.html");

  ASSERT_FALSE(IsUseCounted(WebFeature::kLinkRelPrerenderSameOrigin));
  ASSERT_FALSE(IsUseCounted(WebFeature::kLinkRelPrerenderSameSiteCrossOrigin));
  ASSERT_FALSE(IsUseCounted(WebFeature::kLinkRelPrerenderCrossSite));

  // Add <link rel="prerender"> for a same-origin URL.
  {
    ExecuteScript("createLinkRelPrerender('http://example.com/prerender')");
    ASSERT_EQ(processors().size(), 1u);
    MockNoStatePrefetchProcessor& processor = *processors()[0];

    EXPECT_EQ(KURL("http://example.com/prerender"), processor.Url());
    EXPECT_EQ(mojom::blink::PrerenderTriggerType::kLinkRelPrerender,
              processor.PrerenderTriggerType());

    EXPECT_TRUE(IsUseCounted(WebFeature::kLinkRelPrerenderSameOrigin));
    EXPECT_FALSE(
        IsUseCounted(WebFeature::kLinkRelPrerenderSameSiteCrossOrigin));
    EXPECT_FALSE(IsUseCounted(WebFeature::kLinkRelPrerenderCrossSite));
  }

  // Add <link rel="prerender"> for a same-site cross-origin URL.
  {
    ExecuteScript("createLinkRelPrerender('http://www.example.com/prerender')");
    ASSERT_EQ(processors().size(), 2u);
    MockNoStatePrefetchProcessor& processor = *processors()[1];

    EXPECT_EQ(KURL("http://www.example.com/prerender"), processor.Url());
    EXPECT_EQ(mojom::blink::PrerenderTriggerType::kLinkRelPrerender,
              processor.PrerenderTriggerType());

    EXPECT_TRUE(IsUseCounted(WebFeature::kLinkRelPrerenderSameOrigin));
    EXPECT_TRUE(IsUseCounted(WebFeature::kLinkRelPrerenderSameSiteCrossOrigin));
    EXPECT_FALSE(IsUseCounted(WebFeature::kLinkRelPrerenderCrossSite));
  }

  // Add <link rel="prerender"> for a cross-site URL.
  {
    ExecuteScript("createLinkRelPrerender('https://example.com/prerender')");
    ASSERT_EQ(processors().size(), 3u);
    MockNoStatePrefetchProcessor& processor = *processors()[2];

    EXPECT_EQ(KURL("https://example.com/prerender"), processor.Url());
    EXPECT_EQ(mojom::blink::PrerenderTriggerType::kLinkRelPrerender,
              processor.PrerenderTriggerType());

    EXPECT_TRUE(IsUseCounted(WebFeature::kLinkRelPrerenderSameOrigin));
    EXPECT_TRUE(IsUseCounted(WebFeature::kLinkRelPrerenderSameSiteCrossOrigin));
    EXPECT_TRUE(IsUseCounted(WebFeature::kLinkRelPrerenderCrossSite));
  }
}

}  // namespace blink
