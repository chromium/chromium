// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/internal_popup_menu.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/trees/layer_tree_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/renderer/core/accessibility/ax_context.h"
#include "third_party/blink/renderer/core/css/media_feature_names.h"
#include "third_party/blink/renderer/core/css/media_feature_overrides.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/exported/web_page_popup_impl.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/page_popup_controller.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/web_test_support.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "ui/accessibility/ax_mode.h"

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

// See crbug.com/516936863.
TEST_F(InternalPopupMenuTest, PagePopupControllerUAFAfterOrphanedFree) {
  if (!RuntimeEnabledFeatures::PagePopupEnabled()) {
    return;
  }

  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.Initialize();
  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(web_view->MainFrameImpl(), R"HTML(
    <select id=sel>
      <option>1</option>
      <option>2</option>
    </select>
  )HTML",
                                     base_url);
  Document& document =
      *web_view->MainFrameImpl()->GetDocument().Unwrap<Document>();
  document.View()->UpdateAllLifecyclePhasesForTest();

  auto* sel =
      To<HTMLSelectElement>(document.getElementById(AtomicString("sel")));
  ASSERT_TRUE(sel);
  auto* menu = MakeGarbageCollected<InternalPopupMenu>(
      MakeGarbageCollected<EmptyChromeClient>(), *sel);

  WebPagePopupImpl* popup = web_view->OpenPagePopup(menu);
  popup->DidShowPopup();

  Page* popup_page = popup->GetDocument().Unwrap<Document>()->GetPage();
  ASSERT_TRUE(popup_page);
  Persistent<PagePopupController> controller =
      PagePopupController::From(*popup_page);
  ASSERT_TRUE(controller);

  web_view->CleanupPagePopup();

  test::RunPendingTasks();

  controller->setWindowRect(0, 0, 100, 100);
}

TEST_F(InternalPopupMenuTest, ShowSelectDeviceScaleFactorBelowOne) {
  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.Initialize();
  web_view->MainFrameWidget()->SetDeviceScaleFactorForTesting(0.5f);
  web_view->MainFrameViewWidget()->Resize(gfx::Size(400, 300));

  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(web_view->MainFrameImpl(), R"HTML(
    <!DOCTYPE html>
    <style>
      body { margin: 0; }
      select {
        position: absolute;
        left: 700px;
        top: 500px;
        width: 80px;
        height: 30px;
      }
    </style>
    <select id="select">
      <option>1</option>
      <option>2</option>
    </select>
  )HTML",
                                     base_url);

  Document& document =
      *web_view->MainFrameImpl()->GetDocument().Unwrap<Document>();
  document.View()->UpdateAllLifecyclePhasesForTest();

  auto* select =
      To<HTMLSelectElement>(document.getElementById(AtomicString("select")));
  ASSERT_TRUE(select);

  select->ShowPopup();
  EXPECT_TRUE(select->PopupIsVisible());
}

namespace {
class DisconnectOnChangeListener final : public NativeEventListener {
 public:
  explicit DisconnectOnChangeListener(WebPagePopupImpl* popup)
      : popup_(popup) {}
  void Invoke(ExecutionContext*, Event*) override {
    if (popup_) {
      popup_->WidgetHostDisconnectedForTesting();
      popup_ = nullptr;
    }
  }

 private:
  raw_ptr<WebPagePopupImpl> popup_;
};
}  // namespace

// See crbug.com/565774991.
TEST_F(InternalPopupMenuTest, DisconnectDuringPopupUpdate) {
  if (!RuntimeEnabledFeatures::PagePopupEnabled()) {
    return;
  }

  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.Initialize();
  if (web_view->GetChromeClient().UseExternalPopupMenus()) {
    // <select> doesn't use a WebPagePopupImpl on this platform.
    return;
  }
  web_view->MainFrameViewWidget()->Resize(gfx::Size(800, 600));

  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(web_view->MainFrameImpl(), R"HTML(
    <!DOCTYPE html>
    <style>
      body { margin: 0; }
      select {
        position: absolute;
        left: 0;
        top: 0;
        width: 100px;
        height: 50px;
      }
    </style>
    <select id="select">
      <option>1</option>
      <option>2</option>
    </select>
  )HTML",
                                     base_url);

  Document& document =
      *web_view->MainFrameImpl()->GetDocument().Unwrap<Document>();
  document.View()->UpdateAllLifecyclePhasesForTest();

  auto* select =
      To<HTMLSelectElement>(document.getElementById(AtomicString("select")));
  ASSERT_TRUE(select);

  select->ShowPopup();
  EXPECT_TRUE(select->PopupIsVisible());

  WebPagePopupImpl* popup = web_view->GetPagePopup();
  ASSERT_TRUE(popup);
  popup->DidShowPopup();

  select->ProvisionalSelectionChanged(1);
  select->addEventListener(
      event_type_names::kChange,
      MakeGarbageCollected<DisconnectOnChangeListener>(popup));

  select->SetInlineStyleProperty(CSSPropertyID::kPosition, "absolute");
  select->SetInlineStyleProperty(CSSPropertyID::kTop, "5000px");
  document.View()->UpdateAllLifecyclePhasesForTest();

  web_view->UpdatePagePopup();
}

namespace {
class ShowPickerOnEventListener final : public NativeEventListener {
 public:
  ShowPickerOnEventListener(WebViewImpl* web_view, HTMLInputElement* input)
      : web_view_(web_view), input_(input) {}
  void Invoke(ExecutionContext*, Event*) override {
    if (!input_) {
      return;
    }
    LocalFrame::NotifyUserActivation(
        input_->GetDocument().GetFrame(),
        mojom::blink::UserActivationNotificationType::kTest);
    DummyExceptionStateForTesting exception_state;
    input_->showPicker(exception_state);
    if (web_view_) {
      opened_popup_ = web_view_->GetPagePopup();
      web_view_ = nullptr;
    }
  }
  WebPagePopupImpl* OpenedPopup() const { return opened_popup_.get(); }
  void Trace(Visitor* visitor) const override {
    visitor->Trace(input_);
    NativeEventListener::Trace(visitor);
  }

 private:
  raw_ptr<WebViewImpl> web_view_;
  Member<HTMLInputElement> input_;
  scoped_refptr<WebPagePopupImpl> opened_popup_;
};
}  // namespace

// See crbug.com/566377256.
TEST_F(InternalPopupMenuTest, OpenPopupDuringPagehideOnWebViewClose) {
  if (!RuntimeEnabledFeatures::PagePopupEnabled()) {
    return;
  }

  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.Initialize();
  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(web_view->MainFrameImpl(), R"HTML(
    <!DOCTYPE html>
    <div style="display: none">
      <input id="color" type="color">
      <input id="date" type="date">
    </div>
  )HTML",
                                     base_url);

  Document& document =
      *web_view->MainFrameImpl()->GetDocument().Unwrap<Document>();
  document.View()->UpdateAllLifecyclePhasesForTest();

  Persistent<HTMLInputElement> color =
      To<HTMLInputElement>(document.getElementById(AtomicString("color")));
  ASSERT_TRUE(color);
  Persistent<HTMLInputElement> date =
      To<HTMLInputElement>(document.getElementById(AtomicString("date")));
  ASSERT_TRUE(date);

  auto* color_listener =
      MakeGarbageCollected<ShowPickerOnEventListener>(web_view, color);
  auto* date_listener =
      MakeGarbageCollected<ShowPickerOnEventListener>(web_view, date);
  document.domWindow()->addEventListener(event_type_names::kPagehide,
                                         color_listener);
  document.domWindow()->addEventListener(event_type_names::kPagehide,
                                         date_listener);

  web_view_helper.Reset();
  EXPECT_FALSE(color->IsPickerVisible());
  EXPECT_FALSE(date->IsPickerVisible());
  {
    ScopedWebTestMode web_test_mode(false);
    if (WebPagePopupImpl* popup = color_listener->OpenedPopup()) {
      popup->LayerTreeHostForTesting()->CompositeForTest(
          base::TimeTicks::Now(), false, base::DoNothing());
    }
    if (WebPagePopupImpl* popup = date_listener->OpenedPopup()) {
      popup->LayerTreeHostForTesting()->CompositeForTest(
          base::TimeTicks::Now(), false, base::DoNothing());
    }
  }
  test::RunPendingTasks();
  color.Clear();
  date.Clear();
  ThreadState::Current()->CollectAllGarbageForTesting();
}

// See crbug.com/566377256.
TEST_F(InternalPopupMenuTest, OpenPopupDuringChangeOnWebViewClose) {
  if (!RuntimeEnabledFeatures::PagePopupEnabled()) {
    return;
  }

  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.Initialize();
  if (web_view->GetChromeClient().UseExternalPopupMenus()) {
    return;
  }
  web_view->MainFrameViewWidget()->Resize(gfx::Size(800, 600));

  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(web_view->MainFrameImpl(), R"HTML(
    <!DOCTYPE html>
    <select id="select">
      <option>1</option>
      <option>2</option>
    </select>
    <div style="display: none">
      <input id="color" type="color">
    </div>
  )HTML",
                                     base_url);

  Document& document =
      *web_view->MainFrameImpl()->GetDocument().Unwrap<Document>();
  document.View()->UpdateAllLifecyclePhasesForTest();

  auto* select =
      To<HTMLSelectElement>(document.getElementById(AtomicString("select")));
  ASSERT_TRUE(select);
  Persistent<HTMLInputElement> color =
      To<HTMLInputElement>(document.getElementById(AtomicString("color")));
  ASSERT_TRUE(color);

  select->ShowPopup();
  EXPECT_TRUE(select->PopupIsVisible());

  WebPagePopupImpl* popup = web_view->GetPagePopup();
  ASSERT_TRUE(popup);
  popup->DidShowPopup();

  select->ProvisionalSelectionChanged(1);
  auto* listener =
      MakeGarbageCollected<ShowPickerOnEventListener>(web_view, color);
  select->addEventListener(event_type_names::kChange, listener);

  web_view_helper.Reset();
  EXPECT_FALSE(color->IsPickerVisible());
  if (WebPagePopupImpl* opened_popup = listener->OpenedPopup()) {
    ScopedWebTestMode web_test_mode(false);
    opened_popup->LayerTreeHostForTesting()->CompositeForTest(
        base::TimeTicks::Now(), false, base::DoNothing());
  }
  test::RunPendingTasks();
}

// See crbug.com/566377256.
TEST_F(InternalPopupMenuTest, CompositePopupAfterWebViewClosed) {
  if (!RuntimeEnabledFeatures::PagePopupEnabled()) {
    return;
  }

  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.Initialize();
  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(web_view->MainFrameImpl(), R"HTML(
    <select id="sel">
      <option>1</option>
      <option>2</option>
    </select>
  )HTML",
                                     base_url);
  Document& document =
      *web_view->MainFrameImpl()->GetDocument().Unwrap<Document>();
  document.View()->UpdateAllLifecyclePhasesForTest();

  auto* sel =
      To<HTMLSelectElement>(document.getElementById(AtomicString("sel")));
  ASSERT_TRUE(sel);
  auto* menu = MakeGarbageCollected<InternalPopupMenu>(
      MakeGarbageCollected<EmptyChromeClient>(), *sel);

  scoped_refptr<WebPagePopupImpl> popup = web_view->OpenPagePopup(menu);
  ASSERT_TRUE(popup);
  popup->DidShowPopup();

  web_view->CleanupPagePopup();
  web_view_helper.Reset();

  {
    ScopedWebTestMode web_test_mode(false);
    popup->LayerTreeHostForTesting()->CompositeForTest(
        base::TimeTicks::Now(), false, base::DoNothing());
  }
  popup->ClosePopup();
}

// See crbug.com/566377256.
TEST_F(InternalPopupMenuTest, DetachedInputPopupClosedOnDocumentDetach) {
  if (!RuntimeEnabledFeatures::PagePopupEnabled()) {
    return;
  }

  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.Initialize();
  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(web_view->MainFrameImpl(), R"HTML(
    <!DOCTYPE html>
    <div style="display: none">
      <input id="color" type="color">
    </div>
  )HTML",
                                     base_url);

  Document& document =
      *web_view->MainFrameImpl()->GetDocument().Unwrap<Document>();
  document.View()->UpdateAllLifecyclePhasesForTest();

  Persistent<HTMLInputElement> color =
      To<HTMLInputElement>(document.getElementById(AtomicString("color")));
  ASSERT_TRUE(color);
  color->remove();

  LocalFrame::NotifyUserActivation(
      document.GetFrame(), mojom::blink::UserActivationNotificationType::kTest);
  color->showPicker(ASSERT_NO_EXCEPTION);
  EXPECT_TRUE(color->IsPickerVisible());
  EXPECT_TRUE(web_view->GetPagePopup());

  frame_test_helpers::LoadHTMLString(
      web_view->MainFrameImpl(), "<!DOCTYPE html><p>Next page</p>", base_url);
  EXPECT_FALSE(color->IsPickerVisible());
  EXPECT_FALSE(web_view->GetPagePopup());
}

// See crbug.com/566377256.
TEST_F(InternalPopupMenuTest, DetachedInputPopupClosedOnAdoptNode) {
  if (!RuntimeEnabledFeatures::PagePopupEnabled()) {
    return;
  }

  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.Initialize();
  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(web_view->MainFrameImpl(), R"HTML(
    <!DOCTYPE html>
    <div style="display: none">
      <input id="color" type="color">
      <input id="color2" type="color">
    </div>
  )HTML",
                                     base_url);

  Document& document =
      *web_view->MainFrameImpl()->GetDocument().Unwrap<Document>();
  AXContext ax_context(document, ui::kAXModeComplete);
  document.View()->UpdateAllLifecyclePhasesForTest();

  Persistent<HTMLInputElement> color =
      To<HTMLInputElement>(document.getElementById(AtomicString("color")));
  ASSERT_TRUE(color);
  Persistent<HTMLInputElement> color2 =
      To<HTMLInputElement>(document.getElementById(AtomicString("color2")));
  ASSERT_TRUE(color2);
  color->remove();

  LocalFrame::NotifyUserActivation(
      document.GetFrame(), mojom::blink::UserActivationNotificationType::kTest);
  color->showPicker(ASSERT_NO_EXCEPTION);
  EXPECT_TRUE(color->IsPickerVisible());
  EXPECT_TRUE(web_view->GetPagePopup());

  Document* other_document =
      Document::CreateForTest(*document.GetExecutionContext());
  other_document->adoptNode(color, ASSERT_NO_EXCEPTION);
  EXPECT_FALSE(color->IsPickerVisible());
  EXPECT_FALSE(web_view->GetPagePopup());

  LocalFrame::NotifyUserActivation(
      document.GetFrame(), mojom::blink::UserActivationNotificationType::kTest);
  color2->showPicker(ASSERT_NO_EXCEPTION);
  EXPECT_TRUE(color2->IsPickerVisible());
  EXPECT_TRUE(web_view->GetPagePopup());
}

// list_picker.js focuses the listbox <select> inside the popup before the
// popup has a size. That must not scroll the popup document, or a popup
// narrower than requested hides the start of options.
// See crbug.com/576333008.
TEST_F(InternalPopupMenuTest, NarrowedPopupIsNotScrolled) {
  if (!RuntimeEnabledFeatures::PagePopupEnabled()) {
    return;
  }

  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.Initialize();
  if (web_view->GetChromeClient().UseExternalPopupMenus()) {
    // <select> doesn't use a WebPagePopupImpl on this platform.
    return;
  }
  web_view->MainFrameViewWidget()->Resize(gfx::Size(800, 600));

  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(
      web_view->MainFrameImpl(),
      "<!DOCTYPE html><select id=select><option>" + std::string(2000, 'x') +
          "</option><option>2</option></select>",
      base_url);

  Document& document =
      *web_view->MainFrameImpl()->GetDocument().Unwrap<Document>();
  document.View()->UpdateAllLifecyclePhasesForTest();

  auto* select =
      To<HTMLSelectElement>(document.getElementById(AtomicString("select")));
  ASSERT_TRUE(select);

  select->ShowPopup();
  WebPagePopupImpl* popup = web_view->GetPagePopup();
  ASSERT_TRUE(popup);
  popup->DidShowPopup();

  // Narrow the popup below its content width, like the browser's clamp.
  constexpr gfx::Size kNarrowedSize(300, 100);
  static_cast<WebWidget*>(popup)->Resize(kNarrowedSize);
  LocalFrameView* popup_view = popup->GetDocument().Unwrap<Document>()->View();
  popup_view->UpdateAllLifecyclePhasesForTest();

  PaintLayerScrollableArea* viewport = popup_view->LayoutViewport();
  ASSERT_GT(viewport->ContentsSize().width(), kNarrowedSize.width());
  EXPECT_EQ(0.f, viewport->GetScrollOffset().x());
}

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace blink
