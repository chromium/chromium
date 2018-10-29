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

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
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
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"

using blink::url_test_helpers::ToKURL;

namespace blink {
namespace test {

class MHTMLLoadingTest : public testing::Test {
 public:
  MHTMLLoadingTest() = default;

 protected:
  void SetUp() override { helper_.Initialize(); }

  void TearDown() override {
    platform_->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();
  }

  void RegisterMockedURLLoad(const std::string& url,
                             const std::string& file_name) {
    url_test_helpers::RegisterMockedURLLoad(
        ToKURL(url),
        test::CoreTestDataPath(WebString::FromUTF8("mhtml/" + file_name)),
        WebString::FromUTF8("multipart/related"));
  }

  void LoadURLInTopFrame(const WebURL& url) {
    frame_test_helpers::LoadFrame(helper_.GetWebView()->MainFrameImpl(),
                                  url.GetString().Utf8().data());
  }

  Page* GetPage() const { return helper_.GetWebView()->GetPage(); }

 private:
  frame_test_helpers::WebViewHelper helper_;
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform_;
};

// Checks that the domain is set to the actual MHTML file, not the URL it was
// generated from.
TEST_F(MHTMLLoadingTest, CheckDomain) {
  const char kFileURL[] = "file:///simple_test.mht";

  // Register the mocked frame and load it.
  WebURL url = ToKURL(kFileURL);
  RegisterMockedURLLoad(kFileURL, "simple_test.mht");
  LoadURLInTopFrame(url);
  ASSERT_TRUE(GetPage());
  LocalFrame* frame = ToLocalFrame(GetPage()->MainFrame());
  ASSERT_TRUE(frame);
  Document* document = frame->GetDocument();
  ASSERT_TRUE(document);

  EXPECT_STREQ(kFileURL,
               frame->DomWindow()->location()->toString().Ascii().data());

  const SecurityOrigin* origin = document->GetSecurityOrigin();
  EXPECT_STRNE("localhost", origin->Domain().Ascii().data());
}

// Checks that full sandboxing protection has been turned on.
TEST_F(MHTMLLoadingTest, EnforceSandboxFlags) {
  const char kURL[] = "http://www.example.com";

  // Register the mocked frame and load it.
  RegisterMockedURLLoad(kURL, "page_with_javascript.mht");
  LoadURLInTopFrame(ToKURL(kURL));
  ASSERT_TRUE(GetPage());
  LocalFrame* frame = ToLocalFrame(GetPage()->MainFrame());
  ASSERT_TRUE(frame);
  Document* document = frame->GetDocument();
  ASSERT_TRUE(document);

  // Full sandboxing with the exception to new top-level windows should be
  // turned on.
  EXPECT_EQ(kSandboxAll & ~(kSandboxPopups |
                            kSandboxPropagatesToAuxiliaryBrowsingContexts),
            document->GetSandboxFlags());

  // MHTML document should be loaded into unique origin.
  EXPECT_TRUE(document->GetSecurityOrigin()->IsOpaque());
  // Script execution should be disabled.
  EXPECT_FALSE(document->CanExecuteScripts(kNotAboutToExecuteScript));

  // The element to be created by the script is not there.
  EXPECT_FALSE(document->getElementById("mySpan"));

  // Make sure the subframe is also sandboxed.
  LocalFrame* child_frame =
      ToLocalFrame(GetPage()->MainFrame()->Tree().FirstChild());
  ASSERT_TRUE(child_frame);
  Document* child_document = child_frame->GetDocument();
  ASSERT_TRUE(child_document);

  EXPECT_EQ(kSandboxAll & ~(kSandboxPopups |
                            kSandboxPropagatesToAuxiliaryBrowsingContexts),
            child_document->GetSandboxFlags());

  // MHTML document should be loaded into unique origin.
  EXPECT_TRUE(child_document->GetSecurityOrigin()->IsOpaque());
  // Script execution should be disabled.
  EXPECT_FALSE(child_document->CanExecuteScripts(kNotAboutToExecuteScript));

  // The element to be created by the script is not there.
  EXPECT_FALSE(child_document->getElementById("mySpan"));
}

TEST_F(MHTMLLoadingTest, EnforceSandboxFlagsInXSLT) {
  const char kURL[] = "http://www.example.com";

  // Register the mocked frame and load it.
  RegisterMockedURLLoad(kURL, "xslt.mht");
  LoadURLInTopFrame(ToKURL(kURL));
  ASSERT_TRUE(GetPage());
  LocalFrame* frame = ToLocalFrame(GetPage()->MainFrame());
  ASSERT_TRUE(frame);
  Document* document = frame->GetDocument();
  ASSERT_TRUE(document);

  // Full sandboxing with the exception to new top-level windows should be
  // turned on.
  EXPECT_EQ(kSandboxAll & ~(kSandboxPopups |
                            kSandboxPropagatesToAuxiliaryBrowsingContexts),
            document->GetSandboxFlags());

  // MHTML document should be loaded into unique origin.
  EXPECT_TRUE(document->GetSecurityOrigin()->IsOpaque());
  // Script execution should be disabled.
  EXPECT_FALSE(document->CanExecuteScripts(kNotAboutToExecuteScript));
}

TEST_F(MHTMLLoadingTest, ShadowDom) {
  const char kURL[] = "http://www.example.com";

  // Register the mocked frame and load it.
  RegisterMockedURLLoad(kURL, "shadow.mht");
  LoadURLInTopFrame(ToKURL(kURL));
  ASSERT_TRUE(GetPage());
  LocalFrame* frame = ToLocalFrame(GetPage()->MainFrame());
  ASSERT_TRUE(frame);
  Document* document = frame->GetDocument();
  ASSERT_TRUE(document);

  EXPECT_TRUE(IsShadowHost(document->getElementById("h1")));
  EXPECT_TRUE(IsShadowHost(document->getElementById("h2")));
  // The nested shadow DOM tree is created.
  EXPECT_TRUE(IsShadowHost(
      document->getElementById("h2")->GetShadowRoot()->getElementById("h3")));

  EXPECT_TRUE(IsShadowHost(document->getElementById("h4")));
  // The static element in the shadow dom template is found.
  EXPECT_TRUE(
      document->getElementById("h4")->GetShadowRoot()->getElementById("s1"));
  // The element to be created by the script in the shadow dom template is
  // not found because the script is blocked.
  EXPECT_FALSE(
      document->getElementById("h4")->GetShadowRoot()->getElementById("s2"));
}

TEST_F(MHTMLLoadingTest, FormControlElements) {
  const char kURL[] = "http://www.example.com";

  // Register the mocked frame and load it.
  RegisterMockedURLLoad(kURL, "form.mht");
  LoadURLInTopFrame(ToKURL(kURL));
  ASSERT_TRUE(GetPage());
  LocalFrame* frame = ToLocalFrame(GetPage()->MainFrame());
  ASSERT_TRUE(frame);
  Document* document = frame->GetDocument();
  ASSERT_TRUE(document);

  ClassCollection* formControlElements = document->getElementsByClassName("fc");
  ASSERT_TRUE(formControlElements);
  for (Element* element : *formControlElements)
    EXPECT_TRUE(element->IsDisabledFormControl());

  EXPECT_FALSE(document->getElementById("h1")->IsDisabledFormControl());
  EXPECT_FALSE(document->getElementById("fm")->IsDisabledFormControl());
}

TEST_F(MHTMLLoadingTest, LoadMHTMLContainingSoftLineBreaks) {
  const char kURL[] = "http://www.example.com";

  // Register the mocked frame and load it.
  RegisterMockedURLLoad(kURL, "soft_line_break.mht");
  LoadURLInTopFrame(ToKURL(kURL));
  ASSERT_TRUE(GetPage());
  LocalFrame* frame = ToLocalFrame(GetPage()->MainFrame());
  ASSERT_TRUE(frame);
  // We should not have problem to concatenate header lines separated by soft
  // line breaks.
  Document* document = frame->GetDocument();
  ASSERT_TRUE(document);

  // We should not have problem to concatenate body lines separated by soft
  // line breaks.
  EXPECT_TRUE(document->getElementById(
      "AVeryLongID012345678901234567890123456789012345678901234567890End"));
}

}  // namespace test
}  // namespace blink
