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

class MockPrerender : public mojom::blink::PrerenderHandle {
 public:
  explicit MockPrerender(
      mojom::blink::PrerenderAttributesPtr attributes,
      mojo::PendingRemote<mojom::blink::PrerenderHandleClient> client,
      mojo::PendingReceiver<mojom::blink::PrerenderHandle> handle)
      : attributes_(std::move(attributes)),
        client_(std::move(client)),
        receiver_(this, std::move(handle)) {}
  ~MockPrerender() override = default;

  const KURL& Url() const { return attributes_->url; }
  mojom::blink::PrerenderRelType RelType() const {
    return attributes_->rel_type;
  }

  // Returns the number of times |Cancel| was called.
  size_t CancelCount() const { return cancel_count_; }

  // Returns the number of times |Abandon| was called.
  size_t AbandonCount() const { return abandon_count_; }

  // Used to simulate state changes of the mock prerendered web page. These
  // calls spin the message loop so that the client's receiver side gets a
  // chance to run.
  void NotifyDidStartPrerender() {
    client_->OnPrerenderStart();
    test::RunPendingTasks();
  }
  void NotifyDidSendDOMContentLoadedForPrerender() {
    client_->OnPrerenderDomContentLoaded();
    test::RunPendingTasks();
  }
  void NotifyDidSendLoadForPrerender() {
    client_->OnPrerenderStopLoading();
    test::RunPendingTasks();
  }
  void NotifyDidStopPrerender() {
    client_->OnPrerenderStop();
    test::RunPendingTasks();
  }

  // mojom::blink::PrerenderHandle implementation
  void Cancel() override { cancel_count_++; }
  void Abandon() override { abandon_count_++; }

 private:
  mojom::blink::PrerenderAttributesPtr attributes_;
  mojo::Remote<mojom::blink::PrerenderHandleClient> client_;
  mojo::Receiver<mojom::blink::PrerenderHandle> receiver_;
  size_t cancel_count_ = 0;
  size_t abandon_count_ = 0;
};

class MockPrerenderProcessor : public mojom::blink::PrerenderProcessor {
 public:
  MockPrerenderProcessor() = default;
  ~MockPrerenderProcessor() override = default;

  // Returns the number of times |AddPrerender| was called.
  size_t AddCount() const { return add_count_; }

  std::unique_ptr<MockPrerender> ReleasePrerender() {
    std::unique_ptr<MockPrerender> rv;
    if (!prerenders_.empty()) {
      rv = std::move(prerenders_.front());
      prerenders_.pop();
    }
    return rv;
  }

  void Bind(mojo::ScopedMessagePipeHandle message_pipe_handle) {
    receiver_set_.Add(this,
                      mojo::PendingReceiver<mojom::blink::PrerenderProcessor>(
                          std::move(message_pipe_handle)));
  }

  // mojom::blink::PrerenderProcessor implementation
  void AddPrerender(
      mojom::blink::PrerenderAttributesPtr attributes,
      mojo::PendingRemote<mojom::blink::PrerenderHandleClient> client,
      mojo::PendingReceiver<mojom::blink::PrerenderHandle> handle) override {
    prerenders_.push(std::make_unique<MockPrerender>(
        std::move(attributes), std::move(client), std::move(handle)));
    add_count_++;
  }

 private:
  mojo::ReceiverSet<mojom::blink::PrerenderProcessor> receiver_set_;
  std::queue<std::unique_ptr<MockPrerender>> prerenders_;
  size_t add_count_ = 0;
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
            WTF::BindRepeating(&MockPrerenderProcessor::Bind,
                               WTF::Unretained(&mock_prerender_processor_)));

    frame_test_helpers::LoadFrame(
        web_view_helper_.GetWebView()->MainFrameImpl(),
        std::string(base_url) + file_name);
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

  Element& Console() {
    Document* document =
        web_view_helper_.LocalMainFrame()->GetFrame()->GetDocument();
    Element* console = document->getElementById("console");
    DCHECK(IsA<HTMLUListElement>(console));
    return *console;
  }

  unsigned ConsoleLength() { return Console().CountChildren() - 1; }

  WebString ConsoleAt(unsigned i) {
    DCHECK_GT(ConsoleLength(), i);

    Node* item = NodeTraversal::ChildAt(Console(), 1 + i);

    DCHECK(item);
    DCHECK(IsA<HTMLLIElement>(item));
    DCHECK(item->hasChildren());

    return item->textContent();
  }

  bool IsUseCounted(mojom::WebFeature web_feature) {
    Document* document =
        web_view_helper_.LocalMainFrame()->GetFrame()->GetDocument();
    return document->IsUseCounted(web_feature);
  }

  void ExecuteScript(const char* code) {
    web_view_helper_.LocalMainFrame()->ExecuteScript(
        WebScriptSource(WebString::FromUTF8(code)));
    test::RunPendingTasks();
  }

  TestWebPrerendererClient* PrerendererClient() { return &prerenderer_client_; }
  MockPrerenderProcessor* PrerenderProcessor() {
    return &mock_prerender_processor_;
  }

 private:
  void UnregisterMockPrerenderProcessor() {
    web_view_helper_.LocalMainFrame()
        ->GetFrame()
        ->GetBrowserInterfaceBroker()
        .SetBinderForTesting(mojom::blink::PrerenderProcessor::Name_, {});
  }

  TestWebPrerendererClient prerenderer_client_;
  MockPrerenderProcessor mock_prerender_processor_;

  frame_test_helpers::WebViewHelper web_view_helper_;
};

}  // namespace

TEST_F(PrerenderTest, SinglePrerender) {
  Initialize("http://www.foo.com/", "prerender/single_prerender.html");

  std::unique_ptr<MockPrerender> prerender =
      PrerenderProcessor()->ReleasePrerender();
  EXPECT_TRUE(prerender);
  EXPECT_EQ(KURL("http://prerender.com/"), prerender->Url());
  EXPECT_EQ(mojom::blink::PrerenderRelType::kPrerender, prerender->RelType());

  EXPECT_EQ(1u, PrerenderProcessor()->AddCount());
  EXPECT_EQ(0u, prerender->CancelCount());
  EXPECT_EQ(0u, prerender->AbandonCount());

  EXPECT_FALSE(IsUseCounted(WebFeature::kWebkitPrerenderStartEventFired));
  prerender->NotifyDidStartPrerender();
  EXPECT_EQ(1u, ConsoleLength());
  EXPECT_EQ("webkitprerenderstart", ConsoleAt(0));
  EXPECT_TRUE(IsUseCounted(WebFeature::kWebkitPrerenderStartEventFired));

  EXPECT_FALSE(
      IsUseCounted(WebFeature::kWebkitPrerenderDOMContentLoadedEventFired));
  prerender->NotifyDidSendDOMContentLoadedForPrerender();
  EXPECT_EQ(2u, ConsoleLength());
  EXPECT_EQ("webkitprerenderdomcontentloaded", ConsoleAt(1));
  EXPECT_TRUE(
      IsUseCounted(WebFeature::kWebkitPrerenderDOMContentLoadedEventFired));

  EXPECT_FALSE(IsUseCounted(WebFeature::kWebkitPrerenderLoadEventFired));
  prerender->NotifyDidSendLoadForPrerender();
  EXPECT_EQ(3u, ConsoleLength());
  EXPECT_EQ("webkitprerenderload", ConsoleAt(2));
  EXPECT_TRUE(IsUseCounted(WebFeature::kWebkitPrerenderLoadEventFired));

  EXPECT_FALSE(IsUseCounted(WebFeature::kWebkitPrerenderStopEventFired));
  prerender->NotifyDidStopPrerender();
  EXPECT_EQ(4u, ConsoleLength());
  EXPECT_EQ("webkitprerenderstop", ConsoleAt(3));
  EXPECT_TRUE(IsUseCounted(WebFeature::kWebkitPrerenderStopEventFired));
}

TEST_F(PrerenderTest, CancelPrerender) {
  Initialize("http://www.foo.com/", "prerender/single_prerender.html");

  std::unique_ptr<MockPrerender> prerender =
      PrerenderProcessor()->ReleasePrerender();
  EXPECT_TRUE(prerender);

  EXPECT_EQ(1u, PrerenderProcessor()->AddCount());
  EXPECT_EQ(0u, prerender->CancelCount());
  EXPECT_EQ(0u, prerender->AbandonCount());

  ExecuteScript("removePrerender()");

  EXPECT_EQ(1u, PrerenderProcessor()->AddCount());
  EXPECT_EQ(1u, prerender->CancelCount());
  EXPECT_EQ(0u, prerender->AbandonCount());
}

TEST_F(PrerenderTest, AbandonPrerender) {
  Initialize("http://www.foo.com/", "prerender/single_prerender.html");

  std::unique_ptr<MockPrerender> prerender =
      PrerenderProcessor()->ReleasePrerender();
  EXPECT_TRUE(prerender);

  EXPECT_EQ(1u, PrerenderProcessor()->AddCount());
  EXPECT_EQ(0u, prerender->CancelCount());
  EXPECT_EQ(0u, prerender->AbandonCount());

  NavigateAway();

  EXPECT_EQ(1u, PrerenderProcessor()->AddCount());
  EXPECT_EQ(0u, prerender->CancelCount());
  EXPECT_EQ(0u, prerender->AbandonCount());

  // Check that the prerender does not emit an extra cancel when
  // garbage-collecting everything.
  Close();

  EXPECT_EQ(1u, PrerenderProcessor()->AddCount());
  EXPECT_EQ(0u, prerender->CancelCount());
  EXPECT_EQ(0u, prerender->AbandonCount());
}

TEST_F(PrerenderTest, TwoPrerenders) {
  Initialize("http://www.foo.com/", "prerender/multiple_prerenders.html");

  std::unique_ptr<MockPrerender> first_prerender =
      PrerenderProcessor()->ReleasePrerender();
  EXPECT_TRUE(first_prerender);
  EXPECT_EQ(KURL("http://first-prerender.com/"), first_prerender->Url());

  std::unique_ptr<MockPrerender> second_prerender =
      PrerenderProcessor()->ReleasePrerender();
  EXPECT_TRUE(first_prerender);
  EXPECT_EQ(KURL("http://second-prerender.com/"), second_prerender->Url());

  EXPECT_EQ(2u, PrerenderProcessor()->AddCount());
  EXPECT_EQ(0u, first_prerender->CancelCount());
  EXPECT_EQ(0u, first_prerender->AbandonCount());
  EXPECT_EQ(0u, second_prerender->CancelCount());
  EXPECT_EQ(0u, second_prerender->AbandonCount());

  first_prerender->NotifyDidStartPrerender();
  EXPECT_EQ(1u, ConsoleLength());
  EXPECT_EQ("first_webkitprerenderstart", ConsoleAt(0));

  second_prerender->NotifyDidStartPrerender();
  EXPECT_EQ(2u, ConsoleLength());
  EXPECT_EQ("second_webkitprerenderstart", ConsoleAt(1));
}

TEST_F(PrerenderTest, TwoPrerendersRemovingFirstThenNavigating) {
  Initialize("http://www.foo.com/", "prerender/multiple_prerenders.html");

  std::unique_ptr<MockPrerender> first_prerender =
      PrerenderProcessor()->ReleasePrerender();
  std::unique_ptr<MockPrerender> second_prerender =
      PrerenderProcessor()->ReleasePrerender();

  EXPECT_EQ(2u, PrerenderProcessor()->AddCount());
  EXPECT_EQ(0u, first_prerender->CancelCount());
  EXPECT_EQ(0u, first_prerender->AbandonCount());
  EXPECT_EQ(0u, second_prerender->CancelCount());
  EXPECT_EQ(0u, second_prerender->AbandonCount());

  ExecuteScript("removeFirstPrerender()");

  EXPECT_EQ(2u, PrerenderProcessor()->AddCount());
  EXPECT_EQ(1u, first_prerender->CancelCount());
  EXPECT_EQ(0u, first_prerender->AbandonCount());
  EXPECT_EQ(0u, second_prerender->CancelCount());
  EXPECT_EQ(0u, second_prerender->AbandonCount());

  NavigateAway();

  EXPECT_EQ(2u, PrerenderProcessor()->AddCount());
  EXPECT_EQ(1u, first_prerender->CancelCount());
  EXPECT_EQ(0u, first_prerender->AbandonCount());
  EXPECT_EQ(0u, second_prerender->CancelCount());
  EXPECT_EQ(0u, second_prerender->AbandonCount());
}

TEST_F(PrerenderTest, TwoPrerendersAddingThird) {
  Initialize("http://www.foo.com/", "prerender/multiple_prerenders.html");

  std::unique_ptr<MockPrerender> first_prerender =
      PrerenderProcessor()->ReleasePrerender();
  std::unique_ptr<MockPrerender> second_prerender =
      PrerenderProcessor()->ReleasePrerender();

  EXPECT_EQ(2u, PrerenderProcessor()->AddCount());
  EXPECT_EQ(0u, first_prerender->CancelCount());
  EXPECT_EQ(0u, first_prerender->AbandonCount());
  EXPECT_EQ(0u, second_prerender->CancelCount());
  EXPECT_EQ(0u, second_prerender->AbandonCount());

  ExecuteScript("addThirdPrerender()");

  std::unique_ptr<MockPrerender> third_prerender =
      PrerenderProcessor()->ReleasePrerender();

  EXPECT_EQ(3u, PrerenderProcessor()->AddCount());
  EXPECT_EQ(0u, first_prerender->CancelCount());
  EXPECT_EQ(0u, first_prerender->AbandonCount());
  EXPECT_EQ(0u, second_prerender->CancelCount());
  EXPECT_EQ(0u, second_prerender->AbandonCount());
  EXPECT_EQ(0u, third_prerender->CancelCount());
  EXPECT_EQ(0u, third_prerender->AbandonCount());
}

TEST_F(PrerenderTest, ShortLivedClient) {
  Initialize("http://www.foo.com/", "prerender/single_prerender.html");

  std::unique_ptr<MockPrerender> prerender =
      PrerenderProcessor()->ReleasePrerender();
  EXPECT_TRUE(prerender);

  EXPECT_EQ(1u, PrerenderProcessor()->AddCount());
  EXPECT_EQ(0u, prerender->CancelCount());
  EXPECT_EQ(0u, prerender->AbandonCount());

  NavigateAway();
  Close();

  // This test passes if this next line doesn't crash.
  prerender->NotifyDidStartPrerender();
}

TEST_F(PrerenderTest, FastRemoveElement) {
  Initialize("http://www.foo.com/", "prerender/single_prerender.html");

  std::unique_ptr<MockPrerender> prerender =
      PrerenderProcessor()->ReleasePrerender();
  EXPECT_TRUE(prerender);

  EXPECT_EQ(1u, PrerenderProcessor()->AddCount());
  EXPECT_EQ(0u, prerender->CancelCount());
  EXPECT_EQ(0u, prerender->AbandonCount());

  // Race removing & starting the prerender against each other, as if the
  // element was removed very quickly.
  ExecuteScript("removePrerender()");
  prerender->NotifyDidStartPrerender();

  // The page should be totally disconnected from the Prerender at this point,
  // so the console should not have updated.
  EXPECT_EQ(0u, ConsoleLength());
}

TEST_F(PrerenderTest, MutateTarget) {
  Initialize("http://www.foo.com/", "prerender/single_prerender.html");

  std::unique_ptr<MockPrerender> prerender =
      PrerenderProcessor()->ReleasePrerender();
  EXPECT_TRUE(prerender);
  EXPECT_EQ(KURL("http://prerender.com/"), prerender->Url());

  EXPECT_EQ(1u, PrerenderProcessor()->AddCount());
  EXPECT_EQ(0u, prerender->CancelCount());
  EXPECT_EQ(0u, prerender->AbandonCount());

  // Change the href of this prerender, make sure this is treated as a remove
  // and add.
  ExecuteScript("mutateTarget()");

  std::unique_ptr<MockPrerender> mutated_prerender =
      PrerenderProcessor()->ReleasePrerender();
  EXPECT_EQ(KURL("http://mutated.com/"), mutated_prerender->Url());

  EXPECT_EQ(2u, PrerenderProcessor()->AddCount());
  EXPECT_EQ(1u, prerender->CancelCount());
  EXPECT_EQ(0u, prerender->AbandonCount());
  EXPECT_EQ(0u, mutated_prerender->CancelCount());
  EXPECT_EQ(0u, mutated_prerender->AbandonCount());
}

TEST_F(PrerenderTest, MutateRel) {
  Initialize("http://www.foo.com/", "prerender/single_prerender.html");

  std::unique_ptr<MockPrerender> prerender =
      PrerenderProcessor()->ReleasePrerender();
  EXPECT_TRUE(prerender);
  EXPECT_EQ(KURL("http://prerender.com/"), prerender->Url());

  EXPECT_EQ(1u, PrerenderProcessor()->AddCount());
  EXPECT_EQ(0u, prerender->CancelCount());
  EXPECT_EQ(0u, prerender->AbandonCount());

  // Change the rel of this prerender, make sure this is treated as a remove.
  ExecuteScript("mutateRel()");

  EXPECT_EQ(1u, PrerenderProcessor()->AddCount());
  EXPECT_EQ(1u, prerender->CancelCount());
  EXPECT_EQ(0u, prerender->AbandonCount());
}

}  // namespace blink
