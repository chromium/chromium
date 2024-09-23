/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#include "base/functional/callback_helpers.h"
#include "build/build_config.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/renderer/core/dom/class_collection.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/location.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/mock_policy_container_host.h"
#include "third_party/blink/renderer/platform/loader/static_data_navigation_body_loader.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"

// Note: See also test suite for MHTML document:
// content/browser/navigation_browsertest
// Those have the advantage of running with a real browser process.

using blink::url_test_helpers::ToKURL;

namespace blink {
namespace test {

const network::mojom::blink::WebSandboxFlags kMhtmlSandboxFlags =
    ~network::mojom::blink::WebSandboxFlags::kPopups &
    ~network::mojom::blink::WebSandboxFlags::
        kPropagatesToAuxiliaryBrowsingContexts;

// See the NavigationMhtmlBrowserTest for more up to date tests running with a
// full browser + renderer(s) processes.
class MHTMLLoadingTest : public testing::Test {
 public:
  MHTMLLoadingTest() = default;

 protected:
  void SetUp() override { helper_.Initialize(); }

  void LoadURLInTopFrame(const WebURL& url, const std::string& file_name) {
    std::optional<Vector<char>> data = test::ReadFromFile(
        test::CoreTestDataPath(WebString::FromUTF8("mhtml/" + file_name)));
    ASSERT_TRUE(data);
    scoped_refptr<SharedBuffer> buffer = SharedBuffer::Create(std::move(*data));
    WebLocalFrameImpl* frame = helper_.GetWebView()->MainFrameImpl();
    auto params = std::make_unique<WebNavigationParams>();
    params->url = url;
    params->response = WebURLResponse(url);
    params->response.SetMimeType("multipart/related");
    params->response.SetHttpStatusCode(200);
    params->response.SetExpectedContentLength(buffer->size());
    MockPolicyContainerHost mock_policy_container_host;
    params->policy_container = std::make_unique<blink::WebPolicyContainer>(
        blink::WebPolicyContainerPolicies(),
        mock_policy_container_host.BindNewEndpointAndPassDedicatedRemote());
    params->policy_container->policies.sandbox_flags = kMhtmlSandboxFlags;
    params->body_loader =
        StaticDataNavigationBodyLoader::CreateWithData(std::move(buffer));
    frame->CommitNavigation(std::move(params), nullptr /* extra_data */);
    frame_test_helpers::PumpPendingRequestsForFrameToLoad(frame);
  }

  Page* GetPage() const { return helper_.GetWebView()->GetPage(); }

 private:
  test::TaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform_;
  frame_test_helpers::WebViewHelper helper_;
};

// Checks that the domain is set to the actual MHTML file, not the URL it was
// generated from.
TEST_F(MHTMLLoadingTest, CheckDomain) {
  const char kFileURL[] = "file:///simple_test.mht";

  LoadURLInTopFrame(ToKURL(kFileURL), "simple_test.mht");
  ASSERT_TRUE(GetPage());
  LocalFrame* frame = To<LocalFrame>(GetPage()->MainFrame());
  ASSERT_TRUE(frame);

  EXPECT_EQ(kFileURL, frame->DomWindow()->location()->toString());

  const SecurityOrigin* origin = frame->DomWindow()->GetSecurityOrigin();
  EXPECT_NE("localhost", origin->Domain().Ascii());
}

// Checks that full sandboxing protection has been turned on.
// See also related test: NavigationMhtmlBrowserTest.SandboxedIframe.
TEST_F(MHTMLLoadingTest, EnforceSandboxFlags) {
  const char kURL[] = "http://www.example.com";

  LoadURLInTopFrame(ToKURL(kURL), "page_with_javascript.mht");
  ASSERT_TRUE(GetPage());
  LocalFrame* frame = To<LocalFrame>(GetPage()->MainFrame());
  ASSERT_TRUE(frame);
  LocalDOMWindow* window = frame->DomWindow();
  ASSERT_TRUE(window);

  // Full sandboxing with the exception to new top-level windows should be
  // turned on.
  EXPECT_EQ(kMhtmlSandboxFlags, window->GetSandboxFlags());

  // MHTML document should be loaded into unique origin.
  EXPECT_TRUE(window->GetSecurityOrigin()->IsOpaque());
  // Script execution should be disabled.
  EXPECT_FALSE(window->CanExecuteScripts(kNotAboutToExecuteScript));

  // The element to be created by the script is not there.
  EXPECT_FALSE(window->document()->getElementById(AtomicString("mySpan")));

  // Make sure the subframe is also sandboxed.
  LocalFrame* child_frame =
      To<LocalFrame>(GetPage()->MainFrame()->Tree().FirstChild());
  ASSERT_TRUE(child_frame);
  LocalDOMWindow* child_window = child_frame->DomWindow();
  ASSERT_TRUE(child_window);

  EXPECT_EQ(kMhtmlSandboxFlags, child_window->GetSandboxFlags());

  // MHTML document should be loaded into unique origin.
  EXPECT_TRUE(child_window->GetSecurityOrigin()->IsOpaque());
  // Script execution should be disabled.
  EXPECT_FALSE(child_window->CanExecuteScripts(kNotAboutToExecuteScript));

  // The element to be created by the script is not there.
  EXPECT_FALSE(
      child_window->document()->getElementById(AtomicString("mySpan")));
}

TEST_F(MHTMLLoadingTest, EnforceSandboxFlagsInXSLT) {
  const char kURL[] = "http://www.example.com";

  LoadURLInTopFrame(ToKURL(kURL), "xslt.mht");
  ASSERT_TRUE(GetPage());
  LocalFrame* frame = To<LocalFrame>(GetPage()->MainFrame());
  ASSERT_TRUE(frame);
  LocalDOMWindow* window = frame->DomWindow();
  ASSERT_TRUE(window);

  // Full sandboxing with the exception to new top-level windows should be
  // turned on.
  EXPECT_EQ(kMhtmlSandboxFlags, window->GetSandboxFlags());

  // MHTML document should be loaded into unique origin.
  EXPECT_TRUE(window->GetSecurityOrigin()->IsOpaque());
  // Script execution should be disabled.
  EXPECT_FALSE(window->CanExecuteScripts(kNotAboutToExecuteScript));
}

TEST_F(MHTMLLoadingTest, ShadowDom) {
  const char kURL[] = "http://www.example.com";

  LoadURLInTopFrame(ToKURL(kURL), "shadow.mht");
  ASSERT_TRUE(GetPage());
  LocalFrame* frame = To<LocalFrame>(GetPage()->MainFrame());
  ASSERT_TRUE(frame);
  Document* document = frame->GetDocument();
  ASSERT_TRUE(document);

  EXPECT_TRUE(IsShadowHost(document->getElementById(AtomicString("h2"))));
  // The nested shadow DOM tree is created.
  EXPECT_TRUE(IsShadowHost(document->getElementById(AtomicString("h2"))
                               ->GetShadowRoot()
                               ->getElementById(AtomicString("h3"))));

  EXPECT_TRUE(IsShadowHost(document->getElementById(AtomicString("h4"))));
  // The static element in the shadow dom template is found.
  EXPECT_TRUE(document->getElementById(AtomicString("h4"))
                  ->GetShadowRoot()
                  ->getElementById(AtomicString("s1")));
  // The element to be created by the script in the shadow dom template is
  // not found because the script is blocked.
  EXPECT_FALSE(document->getElementById(AtomicString("h4"))
                   ->GetShadowRoot()
                   ->getElementById(AtomicString("s2")));
}

TEST_F(MHTMLLoadingTest, FormControlElements) {
  const char kURL[] = "http://www.example.com";

  LoadURLInTopFrame(ToKURL(kURL), "form.mht");
  ASSERT_TRUE(GetPage());
  LocalFrame* frame = To<LocalFrame>(GetPage()->MainFrame());
  ASSERT_TRUE(frame);
  Document* document = frame->GetDocument();
  ASSERT_TRUE(document);

  HTMLCollection* formControlElements =
      document->getElementsByClassName(AtomicString("fc"));
  ASSERT_TRUE(formControlElements);
  for (Element* element : *formControlElements)
    EXPECT_TRUE(element->IsDisabledFormControl());

  EXPECT_FALSE(
      document->getElementById(AtomicString("h1"))->IsDisabledFormControl());
  EXPECT_FALSE(
      document->getElementById(AtomicString("fm"))->IsDisabledFormControl());
}

TEST_F(MHTMLLoadingTest, LoadMHTMLContainingSoftLineBreaks) {
  const char kURL[] = "http://www.example.com";

  LoadURLInTopFrame(ToKURL(kURL), "soft_line_break.mht");
  ASSERT_TRUE(GetPage());
  LocalFrame* frame = To<LocalFrame>(GetPage()->MainFrame());
  ASSERT_TRUE(frame);
  // We should not have problem to concatenate header lines separated by soft
  // line breaks.
  Document* document = frame->GetDocument();
  ASSERT_TRUE(document);

  // We should not have problem to concatenate body lines separated by soft
  // line breaks.
  EXPECT_TRUE(document->getElementById(AtomicString(
      "AVeryLongID012345678901234567890123456789012345678901234567890End")));
}

}  // namespace test
}  // namespace blink
