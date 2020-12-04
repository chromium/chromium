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
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/prerender/prerender.mojom-blink.h"
#include "third_party/blink/public/platform/web_cache.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_prerenderer_client.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html_element_type_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

namespace {

class TestWebPrerendererClient : public WebPrerendererClient {
 public:
  TestWebPrerendererClient() = default;
  virtual ~TestWebPrerendererClient() = default;

 private:
  bool IsPrefetchOnly() override { return false; }
};

class MockPrerenderProcessor : public mojom::blink::PrerenderProcessor {
 public:
  explicit MockPrerenderProcessor(
      mojo::PendingReceiver<mojom::blink::PrerenderProcessor>
          pending_receiver) {
    receiver_.Bind(std::move(pending_receiver));
  }
  ~MockPrerenderProcessor() override = default;

  // mojom::blink::PrerenderProcessor implementation
  void Start(mojom::blink::PrerenderAttributesPtr attributes) override {
    attributes_ = std::move(attributes);
  }
  void Cancel() override { cancel_count_++; }

  // Returns the number of times |Cancel| was called.
  size_t CancelCount() const { return cancel_count_; }

  const KURL& Url() const { return attributes_->url; }
  mojom::blink::PrerenderRelType RelType() const {
    return attributes_->rel_type;
  }

 private:
  mojom::blink::PrerenderAttributesPtr attributes_;
  mojo::Receiver<mojom::blink::PrerenderProcessor> receiver_{this};

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
        WebString::FromUTF8(base_url), blink::test::CoreTestDataPath(),
        WebString::FromUTF8(file_name));
    web_view_helper_.Initialize();
    web_view_helper_.GetWebView()->SetPrerendererClient(&prerenderer_client_);

    web_view_helper_.LocalMainFrame()
        ->GetFrame()
        ->GetBrowserInterfaceBroker()
        .SetBinderForTesting(
            mojom::blink::PrerenderProcessor::Name_,
            WTF::BindRepeating(&PrerenderTest::Bind, WTF::Unretained(this)));

    frame_test_helpers::LoadFrame(
        web_view_helper_.GetWebView()->MainFrameImpl(),
        std::string(base_url) + file_name);
  }

  void Bind(mojo::ScopedMessagePipeHandle message_pipe_handle) {
    auto processor = std::make_unique<MockPrerenderProcessor>(
        mojo::PendingReceiver<mojom::blink::PrerenderProcessor>(
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

  std::vector<std::unique_ptr<MockPrerenderProcessor>>& processors() {
    return processors_;
  }

 private:
  void UnregisterMockPrerenderProcessor() {
    web_view_helper_.LocalMainFrame()
        ->GetFrame()
        ->GetBrowserInterfaceBroker()
        .SetBinderForTesting(mojom::blink::PrerenderProcessor::Name_, {});
  }

  std::vector<std::unique_ptr<MockPrerenderProcessor>> processors_;
  mojo::ReceiverSet<mojom::blink::PrerenderProcessor> receiver_set_;

  TestWebPrerendererClient prerenderer_client_;

  frame_test_helpers::WebViewHelper web_view_helper_;
};

}  // namespace

TEST_F(PrerenderTest, SinglePrerender) {
  Initialize("http://www.foo.com/", "prerender/single_prerender.html");
  ASSERT_EQ(processors().size(), 1u);
  MockPrerenderProcessor& processor = *processors()[0];

  EXPECT_EQ(KURL("http://prerender.com/"), processor.Url());
  EXPECT_EQ(mojom::blink::PrerenderRelType::kPrerender, processor.RelType());

  EXPECT_EQ(0u, processor.CancelCount());
}

TEST_F(PrerenderTest, CancelPrerender) {
  Initialize("http://www.foo.com/", "prerender/single_prerender.html");
  ASSERT_EQ(processors().size(), 1u);
  MockPrerenderProcessor& processor = *processors()[0];

  EXPECT_EQ(0u, processor.CancelCount());
  ExecuteScript("removePrerender()");
  EXPECT_EQ(1u, processor.CancelCount());
}

TEST_F(PrerenderTest, TwoPrerenders) {
  Initialize("http://www.foo.com/", "prerender/multiple_prerenders.html");

  ASSERT_EQ(processors().size(), 2u);
  MockPrerenderProcessor& first_processor = *processors()[0];
  EXPECT_EQ(KURL("http://first-prerender.com/"), first_processor.Url());
  MockPrerenderProcessor& second_processor = *processors()[1];
  EXPECT_EQ(KURL("http://second-prerender.com/"), second_processor.Url());

  EXPECT_EQ(0u, first_processor.CancelCount());
  EXPECT_EQ(0u, second_processor.CancelCount());
}

TEST_F(PrerenderTest, TwoPrerendersRemovingFirstThenNavigating) {
  Initialize("http://www.foo.com/", "prerender/multiple_prerenders.html");

  ASSERT_EQ(processors().size(), 2u);
  MockPrerenderProcessor& first_processor = *processors()[0];
  MockPrerenderProcessor& second_processor = *processors()[1];

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
  Initialize("http://www.foo.com/", "prerender/multiple_prerenders.html");

  ASSERT_EQ(processors().size(), 2u);
  MockPrerenderProcessor& first_processor = *processors()[0];
  MockPrerenderProcessor& second_processor = *processors()[1];

  EXPECT_EQ(0u, first_processor.CancelCount());
  EXPECT_EQ(0u, second_processor.CancelCount());

  ExecuteScript("addThirdPrerender()");

  ASSERT_EQ(processors().size(), 3u);
  MockPrerenderProcessor& third_processor = *processors()[2];

  EXPECT_EQ(0u, first_processor.CancelCount());
  EXPECT_EQ(0u, second_processor.CancelCount());
  EXPECT_EQ(0u, third_processor.CancelCount());
}

TEST_F(PrerenderTest, MutateTarget) {
  Initialize("http://www.foo.com/", "prerender/single_prerender.html");
  ASSERT_EQ(processors().size(), 1u);
  MockPrerenderProcessor& processor = *processors()[0];

  EXPECT_EQ(KURL("http://prerender.com/"), processor.Url());

  EXPECT_EQ(0u, processor.CancelCount());

  // Change the href of this prerender, make sure this is treated as a remove
  // and add.
  ExecuteScript("mutateTarget()");

  ASSERT_EQ(processors().size(), 2u);
  MockPrerenderProcessor& mutated_processor = *processors()[1];
  EXPECT_EQ(KURL("http://mutated.com/"), mutated_processor.Url());

  EXPECT_EQ(1u, processor.CancelCount());
  EXPECT_EQ(0u, mutated_processor.CancelCount());
}

TEST_F(PrerenderTest, MutateRel) {
  Initialize("http://www.foo.com/", "prerender/single_prerender.html");
  ASSERT_EQ(processors().size(), 1u);
  MockPrerenderProcessor& processor = *processors()[0];

  EXPECT_EQ(KURL("http://prerender.com/"), processor.Url());

  EXPECT_EQ(0u, processor.CancelCount());

  // Change the rel of this prerender, make sure this is treated as a remove.
  ExecuteScript("mutateRel()");

  EXPECT_EQ(1u, processor.CancelCount());
}

}  // namespace blink
