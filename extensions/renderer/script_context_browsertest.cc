// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/render_frame.h"
#include "content/public/test/frame_load_waiter.h"
#include "extensions/common/script_constants.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "extensions/shell/test/extensions_render_view_test.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "url/gurl.h"

using blink::WebLocalFrame;

namespace extensions {
namespace {

class ScriptContextTest : public ExtensionsRenderViewTest {
 protected:
  GURL GetEffectiveDocumentURLForContext(WebLocalFrame* frame) {
    return ScriptContext::GetEffectiveDocumentURLForContext(
        frame, frame->GetDocument().Url(), /*match_about_blank=*/true);
  }
  GURL GetEffectiveDocumentURLForInjection(
      WebLocalFrame* frame,
      MatchOriginAsFallbackBehavior match_origin_as_fallback =
          MatchOriginAsFallbackBehavior::kAlways) {
    return ScriptContext::GetEffectiveDocumentURLForInjection(
        frame, frame->GetDocument().Url(), match_origin_as_fallback);
  }
};

TEST_F(ScriptContextTest, GetEffectiveDocumentURL) {
  GURL top_url("http://example.com/");
  GURL different_url("http://example.net/");
  GURL blank_url("about:blank");
  GURL srcdoc_url("about:srcdoc");
  GURL data_url("data:text/html,<html>Hi</html>");

  const char frame_html[] =
      R"(<iframe name='frame1' srcdoc="
           <iframe name='frame1_1'></iframe>
           <iframe name='frame1_2' sandbox=''></iframe>
         "></iframe>
         <iframe name='frame2' sandbox='' srcdoc="
           <iframe name='frame2_1'></iframe>
         "></iframe>
         <iframe name='frame3'></iframe>
         <iframe name='frame4' src="data:text/html,<html>Hi</html>"></iframe>
         <iframe name='frame5'
             src="data:text/html,<html>Hi</html>"
             sandbox=''></iframe>)";

  const char frame3_html[] =
      R"(<iframe name='frame3_1'></iframe>
         <iframe name='frame3_2' sandbox=''></iframe>
         <iframe name='frame3_3'
             src="data:text/html,<html>Hi</html>"></iframe>)";

  WebLocalFrame* frame = GetMainFrame();
  ASSERT_TRUE(frame);

  content::RenderFrame::FromWebFrame(frame)->LoadHTMLStringForTesting(
      frame_html, top_url, "UTF-8", GURL(), false /* replace_current_item */);
  content::FrameLoadWaiter(content::RenderFrame::FromWebFrame(frame)).Wait();

  WebLocalFrame* frame1 = frame->FirstChild()->ToWebLocalFrame();
  ASSERT_TRUE(frame1);
  ASSERT_EQ("frame1", frame1->AssignedName());
  WebLocalFrame* frame1_1 = frame1->FirstChild()->ToWebLocalFrame();
  ASSERT_TRUE(frame1_1);
  ASSERT_EQ("frame1_1", frame1_1->AssignedName());
  WebLocalFrame* frame1_2 = frame1_1->NextSibling()->ToWebLocalFrame();
  ASSERT_TRUE(frame1_2);
  ASSERT_EQ("frame1_2", frame1_2->AssignedName());
  WebLocalFrame* frame2 = frame1->NextSibling()->ToWebLocalFrame();
  ASSERT_TRUE(frame2);
  ASSERT_EQ("frame2", frame2->AssignedName());
  WebLocalFrame* frame2_1 = frame2->FirstChild()->ToWebLocalFrame();
  ASSERT_TRUE(frame2_1);
  ASSERT_EQ("frame2_1", frame2_1->AssignedName());
  WebLocalFrame* frame3 = frame2->NextSibling()->ToWebLocalFrame();
  ASSERT_TRUE(frame3);
  ASSERT_EQ("frame3", frame3->AssignedName());
  WebLocalFrame* frame4 = frame3->NextSibling()->ToWebLocalFrame();
  ASSERT_TRUE(frame4);
  ASSERT_EQ("frame4", frame4->AssignedName());
  WebLocalFrame* frame5 = frame4->NextSibling()->ToWebLocalFrame();
  ASSERT_TRUE(frame5);
  ASSERT_EQ("frame5", frame5->AssignedName());

  // Load a blank document in a frame from a different origin.
  content::RenderFrame::FromWebFrame(frame3)->LoadHTMLStringForTesting(
      frame3_html, different_url, "UTF-8", GURL(),
      false /* replace_current_item */);
  content::FrameLoadWaiter(content::RenderFrame::FromWebFrame(frame3)).Wait();

  WebLocalFrame* frame3_1 = frame3->FirstChild()->ToWebLocalFrame();
  ASSERT_TRUE(frame3_1);
  ASSERT_EQ("frame3_1", frame3_1->AssignedName());
  WebLocalFrame* frame3_2 = frame3_1->NextSibling()->ToWebLocalFrame();
  ASSERT_TRUE(frame3_2);
  ASSERT_EQ("frame3_2", frame3_2->AssignedName());
  WebLocalFrame* frame3_3 = frame3_2->NextSibling()->ToWebLocalFrame();
  ASSERT_TRUE(frame3_3);
  ASSERT_EQ("frame3_3", frame3_3->AssignedName());

  // Top-level frame
  EXPECT_EQ(top_url, GetEffectiveDocumentURLForContext(frame));
  EXPECT_EQ(top_url, GetEffectiveDocumentURLForInjection(frame));
  // top -> srcdoc = inherit
  EXPECT_EQ(top_url, GetEffectiveDocumentURLForContext(frame1));
  EXPECT_EQ(top_url, GetEffectiveDocumentURLForInjection(frame1));
  // top -> srcdoc -> about:blank = inherit
  EXPECT_EQ(top_url, GetEffectiveDocumentURLForContext(frame1_1));
  EXPECT_EQ(top_url, GetEffectiveDocumentURLForInjection(frame1_1));
  // top -> srcdoc -> about:blank sandboxed = same URL when classifying
  // contexts, but inherited url when injecting scripts.
  EXPECT_EQ(blank_url, GetEffectiveDocumentURLForContext(frame1_2));
  EXPECT_EQ(top_url, GetEffectiveDocumentURLForInjection(frame1_2));

  // top -> srcdoc [sandboxed] = same URL when classifying contexts,
  // but inherited url when injecting scripts.
  EXPECT_EQ(srcdoc_url, GetEffectiveDocumentURLForContext(frame2));
  EXPECT_EQ(top_url, GetEffectiveDocumentURLForInjection(frame2));
  // top -> srcdoc [sandboxed] -> about:blank = same URL when classifying
  // contexts, but inherited url when injecting scripts.
  EXPECT_EQ(blank_url, GetEffectiveDocumentURLForContext(frame2_1));
  EXPECT_EQ(top_url, GetEffectiveDocumentURLForInjection(frame2_1));

  // top -> data URL = same URL when classifying contexts, but inherited when
  // injecting scripts.
  EXPECT_EQ(data_url, GetEffectiveDocumentURLForContext(frame4));
  EXPECT_EQ(top_url, GetEffectiveDocumentURLForInjection(frame4));
  // Sanity-check: if we only match about: schemes, the original URL should be
  // returned.
  EXPECT_EQ(
      data_url,
      GetEffectiveDocumentURLForInjection(
          frame4,
          MatchOriginAsFallbackBehavior::kMatchForAboutSchemeAndClimbTree));

  // top -> sandboxed data URL = same URL when classifying contexts, but
  // inherited when injecting scripts.
  EXPECT_EQ(data_url, GetEffectiveDocumentURLForContext(frame5));
  EXPECT_EQ(top_url, GetEffectiveDocumentURLForInjection(frame5));
  // Sanity-check: if we only match about: schemes, the original URL should be
  // returned.
  EXPECT_EQ(
      data_url,
      GetEffectiveDocumentURLForInjection(
          frame5,
          MatchOriginAsFallbackBehavior::kMatchForAboutSchemeAndClimbTree));

  // top -> different origin = different origin
  EXPECT_EQ(different_url, GetEffectiveDocumentURLForContext(frame3));
  EXPECT_EQ(different_url, GetEffectiveDocumentURLForInjection(frame3));
  // top -> different origin -> about:blank = inherit (from different origin)
  EXPECT_EQ(different_url, GetEffectiveDocumentURLForContext(frame3_1));
  EXPECT_EQ(different_url, GetEffectiveDocumentURLForInjection(frame3_1));
  // top -> different origin -> about:blank sandboxed = same URL when
  // classifying contexts, but inherited (from different origin) url when
  // injecting scripts.
  EXPECT_EQ(blank_url, GetEffectiveDocumentURLForContext(frame3_2));
  EXPECT_EQ(different_url, GetEffectiveDocumentURLForInjection(frame3_2));
  // top -> different origin -> data URL = same URL when classifying contexts,
  // but inherited (from different origin) url when injecting scripts.
  EXPECT_EQ(data_url, GetEffectiveDocumentURLForContext(frame3_3));
  EXPECT_EQ(different_url, GetEffectiveDocumentURLForInjection(frame3_3));
}

TEST_F(ScriptContextTest, GetMainWorldContextForFrame) {
  // ScriptContextSet::GetMainWorldContextForFrame should work, even without an
  // existing v8::HandleScope.
  content::RenderFrame* render_frame =
      content::RenderFrame::FromWebFrame(GetMainFrame());
  ScriptContext* script_context =
      ScriptContextSet::GetMainWorldContextForFrame(render_frame);
  ASSERT_TRUE(script_context);
  EXPECT_EQ(render_frame, script_context->GetRenderFrame());
}

}  // namespace
}  // namespace extensions
