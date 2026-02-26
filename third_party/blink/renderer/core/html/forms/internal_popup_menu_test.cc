// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/internal_popup_menu.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/renderer/core/css/media_feature_names.h"
#include "third_party/blink/renderer/core/css/media_feature_overrides.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/exported/web_page_popup_impl.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

namespace blink {

// InternalPopupMenuTest is not used on Android, and its Platform implementation
// does not provide the resources (as in GetDataResource) needed by
// InternalPopupMenu::WriteDocument.
#if !BUILDFLAG(IS_ANDROID)

class InternalPopupMenuTest : public PageTestBase {};

TEST_F(InternalPopupMenuTest, ShowSelectDisplayNone) {
  auto dummy_page_holder_ =
      std::make_unique<DummyPageHolder>(gfx::Size(800, 600));
  Document& document = dummy_page_holder_->GetDocument();
  document.body()->SetInnerHTMLWithoutTrustedTypes(R"HTML(
    <div id="container">
      <select id="select">
        <option>1</option>
        <option>2</option>
      </select>
    </div>
  )HTML");
  document.View()->UpdateAllLifecyclePhasesForTest();

  auto* div = document.getElementById(AtomicString("container"));
  auto* select =
      To<HTMLSelectElement>(document.getElementById(AtomicString("select")));
  ASSERT_TRUE(select);
  auto* menu = MakeGarbageCollected<InternalPopupMenu>(
      MakeGarbageCollected<EmptyChromeClient>(), *select);

  div->SetInlineStyleProperty(CSSPropertyID::kDisplay, "none");

  // This call should not cause a crash.
  menu->Show(PopupMenu::kOther);
}

TEST_F(InternalPopupMenuTest, MediaFeatureOverridesPropagation) {
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.Initialize();
  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(web_view->MainFrameImpl(), R"HTML(
    <select id=s1>
      <option>option</option>
    </select>
    <select id=s2>
      <option>option</option>
    </select>
  )HTML",
                                     base_url);
  Document& document =
      *web_view->MainFrameImpl()->GetDocument().Unwrap<Document>();

  web_view->GetPage()->SetMediaFeatureOverride(
      media_feature_names::kPrefersColorSchemeMediaFeature, "light");
  document.View()->UpdateAllLifecyclePhasesForTest();
  auto* s1 = To<HTMLSelectElement>(document.getElementById(AtomicString("s1")));
  auto* menu1 = MakeGarbageCollected<InternalPopupMenu>(
      MakeGarbageCollected<EmptyChromeClient>(), *s1);
  WebPagePopupImpl* popup1 = web_view->OpenPagePopup(menu1);
  popup1->DidShowPopup();
  EXPECT_EQ(popup1->GetDocument()
                .Unwrap<Document>()
                ->GetPage()
                ->GetMediaFeatureOverrides()
                ->GetPreferredColorScheme(),
            mojom::PreferredColorScheme::kLight);
  popup1->ClosePopup();

  web_view->GetPage()->SetMediaFeatureOverride(
      media_feature_names::kPrefersColorSchemeMediaFeature, "dark");
  document.View()->UpdateAllLifecyclePhasesForTest();
  auto* s2 = To<HTMLSelectElement>(document.getElementById(AtomicString("s2")));
  auto* menu2 = MakeGarbageCollected<InternalPopupMenu>(
      MakeGarbageCollected<EmptyChromeClient>(), *s2);
  WebPagePopupImpl* popup2 = web_view->OpenPagePopup(menu2);
  popup2->DidShowPopup();
  EXPECT_EQ(popup2->GetDocument()
                .Unwrap<Document>()
                ->GetPage()
                ->GetMediaFeatureOverrides()
                ->GetPreferredColorScheme(),
            mojom::PreferredColorScheme::kDark);
  popup2->ClosePopup();
}

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace blink
