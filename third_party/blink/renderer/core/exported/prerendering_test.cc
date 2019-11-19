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
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/prerender/prerender_rel_type.h"
#include "third_party/blink/public/platform/web_cache.h"
#include "third_party/blink/public/platform/web_prerender.h"
#include "third_party/blink/public/platform/web_prerendering_support.h"
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

WebURL ToWebURL(const char* url) {
  return WebURL(blink::url_test_helpers::ToKURL(url));
}

class TestWebPrerendererClient : public WebPrerendererClient {
 public:
  TestWebPrerendererClient() = default;
  virtual ~TestWebPrerendererClient() = default;

  void SetExtraDataForNextPrerender(WebPrerender::ExtraData* extra_data) {
    DCHECK(!extra_data_);
    extra_data_ = base::WrapUnique(extra_data);
  }

  WebPrerender ReleaseWebPrerender() {
    DCHECK(!web_prerenders_.empty());
    WebPrerender retval(web_prerenders_.front());
    web_prerenders_.pop_front();
    return retval;
  }

  bool empty() const { return web_prerenders_.empty(); }

  void Clear() { web_prerenders_.clear(); }

 private:
  // From WebPrerendererClient:
  void WillAddPrerender(WebPrerender* prerender) override {
    prerender->SetExtraData(extra_data_.release());

    DCHECK(!prerender->IsNull());
    web_prerenders_.push_back(*prerender);
  }

  bool IsPrefetchOnly() override { return false; }

  std::unique_ptr<WebPrerender::ExtraData> extra_data_;
  std::list<WebPrerender> web_prerenders_;
};

class TestPrerenderingSupport : public WebPrerenderingSupport {
 public:
  TestPrerenderingSupport() { Initialize(this); }

  ~TestPrerenderingSupport() override { Shutdown(); }

  void Clear() {
    added_prerenders_.clear();
    canceled_prerenders_.clear();
    abandoned_prerenders_.clear();
  }

  size_t TotalCount() const {
    return added_prerenders_.size() + canceled_prerenders_.size() +
           abandoned_prerenders_.size();
  }

  size_t AddCount(const WebPrerender& prerender) const {
    return std::count_if(added_prerenders_.begin(), added_prerenders_.end(),
                         [&prerender](const WebPrerender& other) {
                           return other.ToPrerender() ==
                                  prerender.ToPrerender();
                         });
  }

  size_t CancelCount(const WebPrerender& prerender) const {
    return std::count_if(
        canceled_prerenders_.begin(), canceled_prerenders_.end(),
        [&prerender](const WebPrerender& other) {
          return other.ToPrerender() == prerender.ToPrerender();
        });
  }

  size_t AbandonCount(const WebPrerender& prerender) const {
    return std::count_if(
        abandoned_prerenders_.begin(), abandoned_prerenders_.end(),
        [&prerender](const WebPrerender& other) {
          return other.ToPrerender() == prerender.ToPrerender();
        });
  }

 private:
  // From WebPrerenderingSupport:
  void Add(const WebPrerender& prerender) override {
    added_prerenders_.push_back(prerender);
  }

  void Cancel(const WebPrerender& prerender) override {
    canceled_prerenders_.push_back(prerender);
  }

  void Abandon(const WebPrerender& prerender) override {
    abandoned_prerenders_.push_back(prerender);
  }

  void PrefetchFinished() override {}

  Vector<WebPrerender> added_prerenders_;
  Vector<WebPrerender> canceled_prerenders_;
  Vector<WebPrerender> abandoned_prerenders_;
};

class PrerenderingTest : public testing::Test {
 public:
  ~PrerenderingTest() override {
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

    frame_test_helpers::LoadFrame(
        web_view_helper_.GetWebView()->MainFrameImpl(),
        std::string(base_url) + file_name);
  }

  void NavigateAway() {
    frame_test_helpers::LoadFrame(
        web_view_helper_.GetWebView()->MainFrameImpl(), "about:blank");
  }

  void Close() {
    web_view_helper_.LocalMainFrame()->CollectGarbageForTesting();
    web_view_helper_.Reset();

    WebCache::Clear();
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

  void ExecuteScript(const char* code) {
    web_view_helper_.LocalMainFrame()->ExecuteScript(
        WebScriptSource(WebString::FromUTF8(code)));
  }

  TestPrerenderingSupport* PrerenderingSupport() {
    return &prerendering_support_;
  }

  TestWebPrerendererClient* PrerendererClient() { return &prerenderer_client_; }

 private:
  TestPrerenderingSupport prerendering_support_;
  TestWebPrerendererClient prerenderer_client_;

  frame_test_helpers::WebViewHelper web_view_helper_;
};

}  // namespace

TEST_F(PrerenderingTest, SinglePrerender) {
  Initialize("http://www.foo.com/", "prerender/single_prerender.html");

  WebPrerender web_prerender = PrerendererClient()->ReleaseWebPrerender();
  EXPECT_FALSE(web_prerender.IsNull());
  EXPECT_EQ(ToWebURL("http://prerender.com/"), web_prerender.Url());
  EXPECT_EQ(static_cast<unsigned>(kPrerenderRelTypePrerender),
            web_prerender.RelTypes());

  EXPECT_EQ(1u, PrerenderingSupport()->AddCount(web_prerender));
  EXPECT_EQ(1u, PrerenderingSupport()->TotalCount());

  web_prerender.DidStartPrerender();
  EXPECT_EQ(1u, ConsoleLength());
  EXPECT_EQ("webkitprerenderstart", ConsoleAt(0));

  web_prerender.DidSendDOMContentLoadedForPrerender();
  EXPECT_EQ(2u, ConsoleLength());
  EXPECT_EQ("webkitprerenderdomcontentloaded", ConsoleAt(1));

  web_prerender.DidSendLoadForPrerender();
  EXPECT_EQ(3u, ConsoleLength());
  EXPECT_EQ("webkitprerenderload", ConsoleAt(2));

  web_prerender.DidStopPrerender();
  EXPECT_EQ(4u, ConsoleLength());
  EXPECT_EQ("webkitprerenderstop", ConsoleAt(3));
}

TEST_F(PrerenderingTest, CancelPrerender) {
  Initialize("http://www.foo.com/", "prerender/single_prerender.html");

  WebPrerender web_prerender = PrerendererClient()->ReleaseWebPrerender();
  EXPECT_FALSE(web_prerender.IsNull());

  EXPECT_EQ(1u, PrerenderingSupport()->AddCount(web_prerender));
  EXPECT_EQ(1u, PrerenderingSupport()->TotalCount());

  ExecuteScript("removePrerender()");

  EXPECT_EQ(1u, PrerenderingSupport()->CancelCount(web_prerender));
  EXPECT_EQ(2u, PrerenderingSupport()->TotalCount());
}

TEST_F(PrerenderingTest, AbandonPrerender) {
  Initialize("http://www.foo.com/", "prerender/single_prerender.html");

  WebPrerender web_prerender = PrerendererClient()->ReleaseWebPrerender();
  EXPECT_FALSE(web_prerender.IsNull());

  EXPECT_EQ(1u, PrerenderingSupport()->AddCount(web_prerender));
  EXPECT_EQ(1u, PrerenderingSupport()->TotalCount());

  NavigateAway();

  EXPECT_EQ(1u, PrerenderingSupport()->AbandonCount(web_prerender));
  EXPECT_EQ(2u, PrerenderingSupport()->TotalCount());

  // Check that the prerender does not emit an extra cancel when
  // garbage-collecting everything.
  Close();

  EXPECT_EQ(2u, PrerenderingSupport()->TotalCount());
}

TEST_F(PrerenderingTest, ExtraData) {
  class TestExtraData : public WebPrerender::ExtraData {
   public:
    explicit TestExtraData(bool* alive) : alive_(alive) { *alive = true; }

    ~TestExtraData() override { *alive_ = false; }

   private:
    bool* alive_;
  };

  bool alive = false;
  {
    PrerendererClient()->SetExtraDataForNextPrerender(
        new TestExtraData(&alive));
    Initialize("http://www.foo.com/", "prerender/single_prerender.html");
    EXPECT_TRUE(alive);

    WebPrerender web_prerender = PrerendererClient()->ReleaseWebPrerender();

    ExecuteScript("removePrerender()");
    Close();
    PrerenderingSupport()->Clear();
  }
  EXPECT_FALSE(alive);
}

TEST_F(PrerenderingTest, TwoPrerenders) {
  Initialize("http://www.foo.com/", "prerender/multiple_prerenders.html");

  WebPrerender first_prerender = PrerendererClient()->ReleaseWebPrerender();
  EXPECT_FALSE(first_prerender.IsNull());
  EXPECT_EQ(ToWebURL("http://first-prerender.com/"), first_prerender.Url());

  WebPrerender second_prerender = PrerendererClient()->ReleaseWebPrerender();
  EXPECT_FALSE(first_prerender.IsNull());
  EXPECT_EQ(ToWebURL("http://second-prerender.com/"), second_prerender.Url());

  EXPECT_EQ(1u, PrerenderingSupport()->AddCount(first_prerender));
  EXPECT_EQ(1u, PrerenderingSupport()->AddCount(second_prerender));
  EXPECT_EQ(2u, PrerenderingSupport()->TotalCount());

  first_prerender.DidStartPrerender();
  EXPECT_EQ(1u, ConsoleLength());
  EXPECT_EQ("first_webkitprerenderstart", ConsoleAt(0));

  second_prerender.DidStartPrerender();
  EXPECT_EQ(2u, ConsoleLength());
  EXPECT_EQ("second_webkitprerenderstart", ConsoleAt(1));
}

TEST_F(PrerenderingTest, TwoPrerendersRemovingFirstThenNavigating) {
  Initialize("http://www.foo.com/", "prerender/multiple_prerenders.html");

  WebPrerender first_prerender = PrerendererClient()->ReleaseWebPrerender();
  WebPrerender second_prerender = PrerendererClient()->ReleaseWebPrerender();

  EXPECT_EQ(1u, PrerenderingSupport()->AddCount(first_prerender));
  EXPECT_EQ(1u, PrerenderingSupport()->AddCount(second_prerender));
  EXPECT_EQ(2u, PrerenderingSupport()->TotalCount());

  ExecuteScript("removeFirstPrerender()");

  EXPECT_EQ(1u, PrerenderingSupport()->CancelCount(first_prerender));
  EXPECT_EQ(3u, PrerenderingSupport()->TotalCount());

  NavigateAway();

  EXPECT_EQ(1u, PrerenderingSupport()->AbandonCount(second_prerender));
  EXPECT_EQ(4u, PrerenderingSupport()->TotalCount());
}

TEST_F(PrerenderingTest, TwoPrerendersAddingThird) {
  Initialize("http://www.foo.com/", "prerender/multiple_prerenders.html");

  WebPrerender first_prerender = PrerendererClient()->ReleaseWebPrerender();
  WebPrerender second_prerender = PrerendererClient()->ReleaseWebPrerender();

  EXPECT_EQ(1u, PrerenderingSupport()->AddCount(first_prerender));
  EXPECT_EQ(1u, PrerenderingSupport()->AddCount(second_prerender));
  EXPECT_EQ(2u, PrerenderingSupport()->TotalCount());

  ExecuteScript("addThirdPrerender()");

  WebPrerender third_prerender = PrerendererClient()->ReleaseWebPrerender();
  EXPECT_EQ(1u, PrerenderingSupport()->AddCount(third_prerender));
  EXPECT_EQ(3u, PrerenderingSupport()->TotalCount());
}

TEST_F(PrerenderingTest, ShortLivedClient) {
  Initialize("http://www.foo.com/", "prerender/single_prerender.html");

  WebPrerender web_prerender = PrerendererClient()->ReleaseWebPrerender();
  EXPECT_FALSE(web_prerender.IsNull());

  EXPECT_EQ(1u, PrerenderingSupport()->AddCount(web_prerender));
  EXPECT_EQ(1u, PrerenderingSupport()->TotalCount());

  NavigateAway();
  Close();

  // This test passes if this next line doesn't crash.
  web_prerender.DidStartPrerender();
}

TEST_F(PrerenderingTest, FastRemoveElement) {
  Initialize("http://www.foo.com/", "prerender/single_prerender.html");

  WebPrerender web_prerender = PrerendererClient()->ReleaseWebPrerender();
  EXPECT_FALSE(web_prerender.IsNull());

  EXPECT_EQ(1u, PrerenderingSupport()->AddCount(web_prerender));
  EXPECT_EQ(1u, PrerenderingSupport()->TotalCount());

  // Race removing & starting the prerender against each other, as if the
  // element was removed very quickly.
  ExecuteScript("removePrerender()");
  EXPECT_FALSE(web_prerender.IsNull());
  web_prerender.DidStartPrerender();

  // The page should be totally disconnected from the Prerender at this point,
  // so the console should not have updated.
  EXPECT_EQ(0u, ConsoleLength());
}

TEST_F(PrerenderingTest, MutateTarget) {
  Initialize("http://www.foo.com/", "prerender/single_prerender.html");

  WebPrerender web_prerender = PrerendererClient()->ReleaseWebPrerender();
  EXPECT_FALSE(web_prerender.IsNull());
  EXPECT_EQ(ToWebURL("http://prerender.com/"), web_prerender.Url());

  EXPECT_EQ(1u, PrerenderingSupport()->AddCount(web_prerender));
  EXPECT_EQ(0u, PrerenderingSupport()->CancelCount(web_prerender));
  EXPECT_EQ(1u, PrerenderingSupport()->TotalCount());

  // Change the href of this prerender, make sure this is treated as a remove
  // and add.
  ExecuteScript("mutateTarget()");
  EXPECT_EQ(1u, PrerenderingSupport()->CancelCount(web_prerender));

  WebPrerender mutated_prerender = PrerendererClient()->ReleaseWebPrerender();
  EXPECT_EQ(ToWebURL("http://mutated.com/"), mutated_prerender.Url());
  EXPECT_EQ(1u, PrerenderingSupport()->AddCount(web_prerender));
  EXPECT_EQ(1u, PrerenderingSupport()->AddCount(mutated_prerender));
  EXPECT_EQ(3u, PrerenderingSupport()->TotalCount());
}

TEST_F(PrerenderingTest, MutateRel) {
  Initialize("http://www.foo.com/", "prerender/single_prerender.html");

  WebPrerender web_prerender = PrerendererClient()->ReleaseWebPrerender();
  EXPECT_FALSE(web_prerender.IsNull());
  EXPECT_EQ(ToWebURL("http://prerender.com/"), web_prerender.Url());

  EXPECT_EQ(1u, PrerenderingSupport()->AddCount(web_prerender));
  EXPECT_EQ(0u, PrerenderingSupport()->CancelCount(web_prerender));
  EXPECT_EQ(1u, PrerenderingSupport()->TotalCount());

  // Change the rel of this prerender, make sure this is treated as a remove.
  ExecuteScript("mutateRel()");
  EXPECT_EQ(1u, PrerenderingSupport()->CancelCount(web_prerender));
  EXPECT_EQ(2u, PrerenderingSupport()->TotalCount());
}

}  // namespace blink
