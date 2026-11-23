// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/internal_popup_menu.h"

#include <string>
#include <tuple>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/trees/layer_tree_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/web/web_autofill_client.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/renderer/core/accessibility/ax_context.h"
#include "third_party/blink/renderer/core/css/media_feature_names.h"
#include "third_party/blink/renderer/core/css/media_feature_overrides.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/exported/web_page_popup_impl.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/page_popup_client.h"
#include "third_party/blink/renderer/core/page/page_popup_controller.h"
#include "third_party/blink/renderer/core/page/popup_opening_observer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
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

namespace {

class AdoptingPopupOpeningObserver final
    : public GarbageCollected<AdoptingPopupOpeningObserver>,
      public PopupOpeningObserver {
 public:
  AdoptingPopupOpeningObserver(ChromeClient& chrome_client,
                               Document& target_document,
                               Element& element_to_adopt)
      : chrome_client_(&chrome_client),
        target_document_(target_document),
        element_to_adopt_(element_to_adopt) {
    chrome_client_->RegisterPopupOpeningObserver(this);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(chrome_client_);
    visitor->Trace(target_document_);
    visitor->Trace(element_to_adopt_);
  }

  void WillOpenPopup() override {
    chrome_client_->UnregisterPopupOpeningObserver(this);
    target_document_->adoptNode(element_to_adopt_, ASSERT_NO_EXCEPTION);
  }

 private:
  Member<ChromeClient> chrome_client_;
  Member<Document> target_document_;
  Member<Element> element_to_adopt_;
};

}  // namespace

// See crbug.com/580868681.
TEST_F(InternalPopupMenuTest, DatePopupNotOpenedWhenOwnerAdoptedDuringOpen) {
  if (!RuntimeEnabledFeatures::PagePopupEnabled()) {
    return;
  }

  frame_test_helpers::WebViewHelper web_view_helper1;
  WebViewImpl* web_view1 = web_view_helper1.Initialize();
  frame_test_helpers::WebViewHelper web_view_helper2;
  WebViewImpl* web_view2 = web_view_helper2.Initialize();

  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(web_view1->MainFrameImpl(), R"HTML(
    <!DOCTYPE html>
    <input id="date" type="date">
  )HTML",
                                     base_url);
  frame_test_helpers::LoadHTMLString(web_view2->MainFrameImpl(),
                                     "<!DOCTYPE html>", base_url);

  Document& document1 =
      *web_view1->MainFrameImpl()->GetDocument().Unwrap<Document>();
  Document& document2 =
      *web_view2->MainFrameImpl()->GetDocument().Unwrap<Document>();
  document1.View()->UpdateAllLifecyclePhasesForTest();
  document2.View()->UpdateAllLifecyclePhasesForTest();

  auto* date =
      To<HTMLInputElement>(document1.getElementById(AtomicString("date")));
  ASSERT_TRUE(date);

  MakeGarbageCollected<AdoptingPopupOpeningObserver>(
      web_view1->GetChromeClient(), document2, *date);

  auto* listener =
      MakeGarbageCollected<ShowPickerOnEventListener>(web_view1, date);
  document1.domWindow()->addEventListener(event_type_names::kPagehide,
                                          listener);

  LocalFrame::NotifyUserActivation(
      document1.GetFrame(),
      mojom::blink::UserActivationNotificationType::kTest);

  web_view_helper1.Reset();

  EXPECT_FALSE(listener->OpenedPopup());
  if (WebPagePopupImpl* popup = listener->OpenedPopup()) {
    static_cast<WidgetBaseClient*>(popup)->GetURLForDebugTrace();
  }
}

// See crbug.com/580866198.
TEST_F(InternalPopupMenuTest, ColorPopupNotOpenedWhenOwnerAdoptedDuringOpen) {
  if (!RuntimeEnabledFeatures::PagePopupEnabled()) {
    return;
  }

  frame_test_helpers::WebViewHelper web_view_helper1;
  WebViewImpl* web_view1 = web_view_helper1.Initialize();
  frame_test_helpers::WebViewHelper web_view_helper2;
  WebViewImpl* web_view2 = web_view_helper2.Initialize();

  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(web_view1->MainFrameImpl(), R"HTML(
    <!DOCTYPE html>
    <input id="color" type="color">
  )HTML",
                                     base_url);
  frame_test_helpers::LoadHTMLString(web_view2->MainFrameImpl(),
                                     "<!DOCTYPE html>", base_url);

  Document& document1 =
      *web_view1->MainFrameImpl()->GetDocument().Unwrap<Document>();
  Document& document2 =
      *web_view2->MainFrameImpl()->GetDocument().Unwrap<Document>();
  document1.View()->UpdateAllLifecyclePhasesForTest();
  document2.View()->UpdateAllLifecyclePhasesForTest();

  auto* color =
      To<HTMLInputElement>(document1.getElementById(AtomicString("color")));
  ASSERT_TRUE(color);

  MakeGarbageCollected<AdoptingPopupOpeningObserver>(
      web_view1->GetChromeClient(), document2, *color);

  auto* listener =
      MakeGarbageCollected<ShowPickerOnEventListener>(web_view1, color);
  document1.domWindow()->addEventListener(event_type_names::kPagehide,
                                          listener);

  LocalFrame::NotifyUserActivation(
      document1.GetFrame(),
      mojom::blink::UserActivationNotificationType::kTest);

  web_view_helper1.Reset();

  EXPECT_FALSE(listener->OpenedPopup());
  if (WebPagePopupImpl* popup = listener->OpenedPopup()) {
    static_cast<WidgetBaseClient*>(popup)->GetURLForDebugTrace();
  }
}

namespace {

class PopupClosingAutofillClient final : public WebAutofillClient {
 public:
  enum class Action {
    kNone,
    kRemoveOwnerOnly,
    kDisconnectWidgetOnly,
    kRemoveOwnerAndDisconnectWidget,
  };

  PopupClosingAutofillClient(Element* owner,
                             WebPagePopupImpl* popup,
                             Action action)
      : owner_(owner), popup_(popup), action_(action) {}

  void UserGestureObserved() override {
    ++gesture_count_;
    if (action_ == Action::kNone) {
      return;
    }
    WebPagePopupImpl* popup = std::exchange(popup_, nullptr);
    Element* owner = owner_.Release();
    if (owner && (action_ == Action::kRemoveOwnerOnly ||
                  action_ == Action::kRemoveOwnerAndDisconnectWidget)) {
      owner->remove();
    }
    if (popup && (action_ == Action::kDisconnectWidgetOnly ||
                  action_ == Action::kRemoveOwnerAndDisconnectWidget)) {
      popup->WidgetHostDisconnectedForTesting();
    }
  }

  int GestureCount() const { return gesture_count_; }

 private:
  Persistent<Element> owner_;
  raw_ptr<WebPagePopupImpl> popup_;
  const Action action_;
  int gesture_count_ = 0;
};

enum class PopupInputDispatchKind {
  kPopupMouseDown,
  kPopupGestureTap,
  kPopupGestureTapDown,
  kPopupKeyDown,
  kOpenerForwardedKeyDown,
};

std::string ActionToString(PopupClosingAutofillClient::Action action) {
  switch (action) {
    case PopupClosingAutofillClient::Action::kNone:
      return "None";
    case PopupClosingAutofillClient::Action::kRemoveOwnerOnly:
      return "RemoveOwnerOnly";
    case PopupClosingAutofillClient::Action::kDisconnectWidgetOnly:
      return "DisconnectWidgetOnly";
    case PopupClosingAutofillClient::Action::kRemoveOwnerAndDisconnectWidget:
      return "RemoveOwnerAndDisconnectWidget";
  }
}

std::string DispatchKindToString(PopupInputDispatchKind dispatch_kind) {
  switch (dispatch_kind) {
    case PopupInputDispatchKind::kPopupMouseDown:
      return "PopupMouseDown";
    case PopupInputDispatchKind::kPopupGestureTap:
      return "PopupGestureTap";
    case PopupInputDispatchKind::kPopupGestureTapDown:
      return "PopupGestureTapDown";
    case PopupInputDispatchKind::kPopupKeyDown:
      return "PopupKeyDown";
    case PopupInputDispatchKind::kOpenerForwardedKeyDown:
      return "OpenerForwardedKeyDown";
  }
}

}  // namespace

class InternalPopupMenuDisconnectTest
    : public InternalPopupMenuTest,
      public testing::WithParamInterface<
          std::tuple<PopupClosingAutofillClient::Action,
                     PopupInputDispatchKind>> {};

// See crbug.com/585726394.
TEST_P(InternalPopupMenuDisconnectTest,
       DisconnectDuringPopupInputUserActivation) {
  if (!RuntimeEnabledFeatures::PagePopupEnabled()) {
    return;
  }

  const auto [action, dispatch_kind] = GetParam();

  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.Initialize();
  web_view->MainFrameViewWidget()->Resize(gfx::Size(800, 600));

  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(web_view->MainFrameImpl(), R"HTML(
    <!DOCTYPE html>
    <input id="date" type="date" value="2020-01-15">
  )HTML",
                                     base_url);

  Document& document =
      *web_view->MainFrameImpl()->GetDocument().Unwrap<Document>();
  document.View()->UpdateAllLifecyclePhasesForTest();

  Persistent<HTMLInputElement> date =
      To<HTMLInputElement>(document.getElementById(AtomicString("date")));
  ASSERT_TRUE(date);

  LocalFrame::NotifyUserActivation(
      document.GetFrame(), mojom::blink::UserActivationNotificationType::kTest);
  date->showPicker(ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(date->IsPickerVisible());

  WebPagePopupImpl* popup = web_view->GetPagePopup();
  ASSERT_TRUE(popup);
  Persistent<LocalFrame> popup_frame =
      popup->GetDocument().Unwrap<Document>()->GetFrame();
  ASSERT_TRUE(popup_frame);
  popup->DidShowPopup();
  popup->SetScreenRects(gfx::Rect(0, 0, 200, 200), gfx::Rect(0, 0, 200, 200));
  LocalFrame::ConsumeTransientUserActivation(document.GetFrame());

  PopupClosingAutofillClient autofill_client(date, popup, action);
  web_view->MainFrameImpl()->SetAutofillClient(&autofill_client);

  switch (dispatch_kind) {
    case PopupInputDispatchKind::kPopupMouseDown: {
      WebMouseEvent mouse_event(WebInputEvent::Type::kMouseDown,
                                WebInputEvent::kNoModifiers,
                                WebInputEvent::GetStaticTimeStampForTests());
      mouse_event.button = WebMouseEvent::Button::kLeft;
      mouse_event.SetPositionInWidget(10, 10);
      mouse_event.SetPositionInScreen(10, 10);
      mouse_event.click_count = 1;
      popup->ProcessInputEventSynchronouslyForTesting(
          WebCoalescedInputEvent(mouse_event, {}));
      EXPECT_FALSE(popup_frame->GetEventHandler().MousePressed());
      break;
    }
    case PopupInputDispatchKind::kPopupGestureTap: {
      WebGestureEvent gesture_event(WebInputEvent::Type::kGestureTap,
                                    WebInputEvent::kNoModifiers,
                                    WebInputEvent::GetStaticTimeStampForTests(),
                                    WebGestureDevice::kTouchscreen);
      gesture_event.SetPositionInWidget(gfx::PointF(10, 10));
      gesture_event.SetPositionInScreen(gfx::PointF(10, 10));
      popup->ProcessInputEventSynchronouslyForTesting(
          WebCoalescedInputEvent(gesture_event, {}));
      break;
    }
    case PopupInputDispatchKind::kPopupGestureTapDown: {
      WebGestureEvent gesture_event(WebInputEvent::Type::kGestureTapDown,
                                    WebInputEvent::kNoModifiers,
                                    WebInputEvent::GetStaticTimeStampForTests(),
                                    WebGestureDevice::kTouchscreen);
      gesture_event.SetPositionInWidget(gfx::PointF(10, 10));
      gesture_event.SetPositionInScreen(gfx::PointF(10, 10));
      popup->ProcessInputEventSynchronouslyForTesting(
          WebCoalescedInputEvent(gesture_event, {}));
      break;
    }
    case PopupInputDispatchKind::kPopupKeyDown: {
      WebKeyboardEvent key_event(WebInputEvent::Type::kRawKeyDown,
                                 WebInputEvent::kNoModifiers,
                                 WebInputEvent::GetStaticTimeStampForTests());
      key_event.windows_key_code = VKEY_DOWN;
      popup->ProcessInputEventSynchronouslyForTesting(
          WebCoalescedInputEvent(key_event, {}));
      break;
    }
    case PopupInputDispatchKind::kOpenerForwardedKeyDown: {
      WebKeyboardEvent key_event(WebInputEvent::Type::kRawKeyDown,
                                 WebInputEvent::kNoModifiers,
                                 WebInputEvent::GetStaticTimeStampForTests());
      key_event.windows_key_code = VKEY_DOWN;
      web_view->MainFrameViewWidget()->ProcessInputEventSynchronouslyForTesting(
          WebCoalescedInputEvent(key_event, {}));
      break;
    }
  }

  web_view->MainFrameImpl()->SetAutofillClient(nullptr);
  EXPECT_EQ(1, autofill_client.GestureCount());
  EXPECT_TRUE(LocalFrame::HasTransientUserActivation(document.GetFrame()));
  EXPECT_FALSE(date->IsPickerVisible());
  EXPECT_FALSE(web_view->GetPagePopup());

  if (action == PopupClosingAutofillClient::Action::kRemoveOwnerOnly) {
    popup->WidgetHostDisconnectedForTesting();
  }
  test::RunPendingTasks();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    InternalPopupMenuDisconnectTest,
    testing::Combine(
        testing::Values(
            PopupClosingAutofillClient::Action::kRemoveOwnerAndDisconnectWidget,
            PopupClosingAutofillClient::Action::kDisconnectWidgetOnly,
            PopupClosingAutofillClient::Action::kRemoveOwnerOnly),
        testing::Values(PopupInputDispatchKind::kPopupMouseDown,
                        PopupInputDispatchKind::kPopupGestureTap,
                        PopupInputDispatchKind::kPopupGestureTapDown,
                        PopupInputDispatchKind::kPopupKeyDown,
                        PopupInputDispatchKind::kOpenerForwardedKeyDown)),
    [](const testing::TestParamInfo<InternalPopupMenuDisconnectTest::ParamType>&
           info) {
      return ActionToString(std::get<0>(info.param)) + "_" +
             DispatchKindToString(std::get<1>(info.param));
    });

TEST_F(InternalPopupMenuTest, PopupRemainsOpenAfterInputUserActivation) {
  if (!RuntimeEnabledFeatures::PagePopupEnabled()) {
    return;
  }

  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.Initialize();
  web_view->MainFrameViewWidget()->Resize(gfx::Size(800, 600));

  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(web_view->MainFrameImpl(), R"HTML(
    <!DOCTYPE html>
    <input id="date" type="date" value="2020-01-15">
  )HTML",
                                     base_url);

  Document& document =
      *web_view->MainFrameImpl()->GetDocument().Unwrap<Document>();
  document.View()->UpdateAllLifecyclePhasesForTest();

  Persistent<HTMLInputElement> date =
      To<HTMLInputElement>(document.getElementById(AtomicString("date")));
  ASSERT_TRUE(date);

  LocalFrame::NotifyUserActivation(
      document.GetFrame(), mojom::blink::UserActivationNotificationType::kTest);
  date->showPicker(ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(date->IsPickerVisible());

  WebPagePopupImpl* popup = web_view->GetPagePopup();
  ASSERT_TRUE(popup);
  Persistent<LocalFrame> popup_frame =
      popup->GetDocument().Unwrap<Document>()->GetFrame();
  ASSERT_TRUE(popup_frame);
  popup->DidShowPopup();
  popup->SetScreenRects(gfx::Rect(0, 0, 200, 200), gfx::Rect(0, 0, 200, 200));
  LocalFrame::ConsumeTransientUserActivation(document.GetFrame());

  PopupClosingAutofillClient autofill_client(
      nullptr, nullptr, PopupClosingAutofillClient::Action::kNone);
  web_view->MainFrameImpl()->SetAutofillClient(&autofill_client);

  WebMouseEvent mouse_event(WebInputEvent::Type::kMouseDown,
                            WebInputEvent::kNoModifiers,
                            WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = WebMouseEvent::Button::kLeft;
  mouse_event.SetPositionInWidget(1, 1);
  mouse_event.SetPositionInScreen(1, 1);
  mouse_event.click_count = 1;
  popup->ProcessInputEventSynchronouslyForTesting(
      WebCoalescedInputEvent(mouse_event, {}));
  EXPECT_TRUE(popup_frame->GetEventHandler().MousePressed());

  WebKeyboardEvent key_event(WebInputEvent::Type::kRawKeyDown,
                             WebInputEvent::kNoModifiers,
                             WebInputEvent::GetStaticTimeStampForTests());
  key_event.windows_key_code = VKEY_SHIFT;
  popup->ProcessInputEventSynchronouslyForTesting(
      WebCoalescedInputEvent(key_event, {}));

  web_view->MainFrameImpl()->SetAutofillClient(nullptr);
  EXPECT_EQ(2, autofill_client.GestureCount());
  EXPECT_TRUE(LocalFrame::HasTransientUserActivation(document.GetFrame()));
  EXPECT_TRUE(date->IsPickerVisible());
  EXPECT_EQ(popup, web_view->GetPagePopup());

  popup->ClosePopup();
  popup->WidgetHostDisconnectedForTesting();
  test::RunPendingTasks();
}

namespace {

class NestedRunLoopOnBlurListener final : public NativeEventListener {
 public:
  void Invoke(ExecutionContext*, Event*) override {
    ++blur_count_;
    test::RunPendingTasks();
  }

  int BlurCount() const { return blur_count_; }

 private:
  int blur_count_ = 0;
};

class NestedRunLoopPagePopupClient final
    : public GarbageCollected<NestedRunLoopPagePopupClient>,
      public PagePopupClient {
 public:
  NestedRunLoopPagePopupClient(ChromeClient& chrome_client, Element& owner)
      : chrome_client_(chrome_client), owner_(owner) {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(chrome_client_);
    visitor->Trace(owner_);
    PagePopupClient::Trace(visitor);
  }

  void WriteDocument(SegmentedBuffer& data) override {
    PagePopupClient::AddLiteral("<!DOCTYPE html><body></body>", data);
    // Spin a nested run loop so the Mojo PopupWidgetHost disconnect (from
    // FakeLocalFrameHost dropping the receiver) is delivered while
    // WebPagePopupImpl is still being constructed/initialized.
    test::RunPendingTasks();
  }

  Element& OwnerElement() override { return *owner_; }
  ChromeClient& GetChromeClient() override { return *chrome_client_; }
  Locale& GetLocale() override { return Locale::DefaultLocale(); }
  void SetValueAndClosePopup(int, const String&, bool) override {}
  void SetValue(const String&) override {}
  void CancelPopup() override { ++cancel_count_; }
  void DidClosePopup() override { ++did_close_count_; }

  int CancelCount() const { return cancel_count_; }
  int DidCloseCount() const { return did_close_count_; }

 private:
  Member<ChromeClient> chrome_client_;
  Member<Element> owner_;
  int cancel_count_ = 0;
  int did_close_count_ = 0;
};

}  // namespace

// Regression test for crbug.com/586391223: if a nested run loop delivers the
// PopupWidgetHost disconnect while WebPagePopupImpl is still being initialized,
// it must not self-delete mid-construction or leave a dangling pointer in
// WebViewImpl.
TEST_F(InternalPopupMenuTest, DisconnectDuringPopupInitialization) {
  if (!RuntimeEnabledFeatures::PagePopupEnabled()) {
    return;
  }

  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.Initialize();
  web_view->MainFrameViewWidget()->Resize(gfx::Size(800, 600));

  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(web_view->MainFrameImpl(), R"HTML(
    <!DOCTYPE html>
    <input id="date" type="date">
  )HTML",
                                     base_url);

  Document& document =
      *web_view->MainFrameImpl()->GetDocument().Unwrap<Document>();
  document.View()->UpdateAllLifecyclePhasesForTest();

  Element* date = document.getElementById(AtomicString("date"));
  ASSERT_TRUE(date);

  auto* client = MakeGarbageCollected<NestedRunLoopPagePopupClient>(
      web_view->GetChromeClient(), *date);
  WebPagePopupImpl* popup = web_view->OpenPagePopup(client);
  EXPECT_EQ(nullptr, popup);
  EXPECT_EQ(nullptr, web_view->GetPagePopup());
  EXPECT_EQ(1, client->CancelCount());
  test::RunPendingTasks();
}

// Regression test for crbug.com/586391223 and crbug.com/574469802: a forced
// opener layout during popup creation must not run script during slot
// assignment recalc when moving an iframe with a previewed temporal input into
// a <canvas> subtree. Clearing the suggested value (which rebuilds fields and
// fires blur) is deferred to the document lifecycle before paint.
TEST_F(InternalPopupMenuTest, DisconnectDuringPopupConstruction) {
  if (!RuntimeEnabledFeatures::PagePopupEnabled()) {
    return;
  }

  // Production Chrome has OmitBlurEventOnElementRemoval disabled (status:
  // "test"), so removing the focused date-time sub-field fires `blur`. Disable
  // the test-only flag here to match shipping behavior.
  ScopedOmitBlurEventOnElementRemovalForTest omit_blur(false);

  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.Initialize();
  web_view->MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  web_view->MainFrameViewWidget()->SetFocus(true);

  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(web_view->MainFrameImpl(), R"HTML(
    <!DOCTYPE html>
    <div id="host">
      <template shadowrootmode="open">
        <slot name="a"></slot>
        <canvas><slot name="b"></slot></canvas>
      </template>
      <iframe id="child" slot="a"></iframe>
    </div>
  )HTML",
                                     base_url);

  WebLocalFrameImpl* child_frame =
      To<WebLocalFrameImpl>(web_view->MainFrameImpl()->FirstChild());
  ASSERT_TRUE(child_frame);
  frame_test_helpers::LoadHTMLString(child_frame, R"HTML(
    <!DOCTYPE html>
    <input id="month" type="month">
  )HTML",
                                     base_url);

  Document& top_document =
      *web_view->MainFrameImpl()->GetDocument().Unwrap<Document>();
  Document& child_document = *child_frame->GetDocument().Unwrap<Document>();
  top_document.View()->UpdateAllLifecyclePhasesForTest();

  Element* iframe = top_document.getElementById(AtomicString("child"));
  ASSERT_TRUE(iframe);
  Persistent<HTMLInputElement> month = To<HTMLInputElement>(
      child_document.getElementById(AtomicString("month")));
  ASSERT_TRUE(month);

  // Give the temporal input a dirty value and a previewed suggested value,
  // focus one of its sub-fields, and then make its cached field layout
  // parameters stale via the `value` attribute (step base).
  month->SetValue("2026-01");
  month->SetSuggestedValue(String("2026-02"));
  month->Focus();
  ASSERT_TRUE(child_document.FocusedElement());
  month->setAttribute(html_names::kValueAttr, AtomicString("2026-03"));

  auto* blur_listener = MakeGarbageCollected<NestedRunLoopOnBlurListener>();
  month->addEventListener(event_type_names::kBlur, blur_listener);

  // Move the iframe into the slot inside <canvas> without flushing layout.
  iframe->setAttribute(html_names::kSlotAttr, AtomicString("b"));

  LocalFrame::NotifyUserActivation(
      child_document.GetFrame(),
      mojom::blink::UserActivationNotificationType::kTest);
  month->showPicker(ASSERT_NO_EXCEPTION);

  // Slot assignment recalc during forced layout defers clearing the suggested
  // value, so blur must not fire synchronously during popup construction.
  EXPECT_EQ(0, blur_listener->BlurCount());
  EXPECT_FALSE(month->SuggestedValue().IsNull());

  top_document.View()->UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(1, blur_listener->BlurCount());
  EXPECT_TRUE(month->SuggestedValue().IsNull());

  test::RunPendingTasks();
  EXPECT_FALSE(month->IsPickerVisible());
  EXPECT_FALSE(web_view->GetPagePopup());
}

namespace {

class SelfClosingPagePopupClient final
    : public GarbageCollected<SelfClosingPagePopupClient>,
      public PagePopupClient {
 public:
  SelfClosingPagePopupClient(ChromeClient& chrome_client, Element& owner)
      : chrome_client_(chrome_client), owner_(owner) {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(chrome_client_);
    visitor->Trace(owner_);
    PagePopupClient::Trace(visitor);
  }

  void WriteDocument(SegmentedBuffer& data) override {
    PagePopupClient::AddLiteral(
        "<!DOCTYPE html><body><script>window.close();</script></body>", data);
  }

  Element& OwnerElement() override { return *owner_; }
  ChromeClient& GetChromeClient() override { return *chrome_client_; }
  Locale& GetLocale() override { return Locale::DefaultLocale(); }
  void SetValueAndClosePopup(int, const String&, bool) override {}
  void SetValue(const String&) override {}
  void CancelPopup() override {}
  void DidClosePopup() override { ++did_close_count_; }

  int DidCloseCount() const { return did_close_count_; }

 private:
  Member<ChromeClient> chrome_client_;
  Member<Element> owner_;
  int did_close_count_ = 0;
};

class ChangeInputTypeOnEventListener final : public NativeEventListener {
 public:
  explicit ChangeInputTypeOnEventListener(HTMLInputElement* input)
      : input_(input) {}

  void Invoke(ExecutionContext*, Event*) override {
    ++count_;
    if (input_) {
      input_->setType(input_type_names::kText);
    }
  }

  int Count() const { return count_; }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(input_);
    NativeEventListener::Trace(visitor);
  }

 private:
  Member<HTMLInputElement> input_;
  int count_ = 0;
};

}  // namespace

// Regression test for crbug.com/586391223: if popup script closes the popup
// synchronously during ForceSynchronousDocumentInstall(), OpenPagePopup() must
// not return a closed popup or leak its self-reference.
TEST_F(InternalPopupMenuTest, PopupClosedByScriptDuringInitialization) {
  if (!RuntimeEnabledFeatures::PagePopupEnabled()) {
    return;
  }

  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.Initialize();
  web_view->MainFrameViewWidget()->Resize(gfx::Size(800, 600));

  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(web_view->MainFrameImpl(), R"HTML(
    <!DOCTYPE html>
    <input id="date" type="date">
  )HTML",
                                     base_url);

  Document& document =
      *web_view->MainFrameImpl()->GetDocument().Unwrap<Document>();
  document.View()->UpdateAllLifecyclePhasesForTest();

  Element* date = document.getElementById(AtomicString("date"));
  ASSERT_TRUE(date);

  auto* client = MakeGarbageCollected<SelfClosingPagePopupClient>(
      web_view->GetChromeClient(), *date);
  WebPagePopupImpl* popup = web_view->OpenPagePopup(client);
  EXPECT_EQ(nullptr, popup);
  EXPECT_EQ(nullptr, web_view->GetPagePopup());
  EXPECT_EQ(1, client->DidCloseCount());
  test::RunPendingTasks();
}

// Regression test for crbug.com/586391223: synchronous script during
// CancelPagePopup() may change the opening date input's type, clearing its
// PickerIndicatorOwner before the popup is created.
TEST_F(InternalPopupMenuTest, OpenDatePopupWhenOwnerChangesTypeDuringCancel) {
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
    <input id="date" type="date">
  )HTML",
                                     base_url);

  Document& document =
      *web_view->MainFrameImpl()->GetDocument().Unwrap<Document>();
  document.View()->UpdateAllLifecyclePhasesForTest();

  auto* select =
      To<HTMLSelectElement>(document.getElementById(AtomicString("select")));
  ASSERT_TRUE(select);
  auto* date =
      To<HTMLInputElement>(document.getElementById(AtomicString("date")));
  ASSERT_TRUE(date);

  select->ShowPopup();
  ASSERT_TRUE(select->PopupIsVisible());
  ASSERT_TRUE(web_view->GetPagePopup());
  web_view->GetPagePopup()->DidShowPopup();

  select->ProvisionalSelectionChanged(1);
  auto* listener = MakeGarbageCollected<ChangeInputTypeOnEventListener>(date);
  select->addEventListener(event_type_names::kChange, listener);

  LocalFrame::NotifyUserActivation(
      document.GetFrame(), mojom::blink::UserActivationNotificationType::kTest);
  date->showPicker(ASSERT_NO_EXCEPTION);

  EXPECT_EQ(1, listener->Count());
  EXPECT_EQ(input_type_names::kText, date->type());
  EXPECT_FALSE(date->IsPickerVisible());
  EXPECT_EQ(nullptr, web_view->GetPagePopup());
  test::RunPendingTasks();
}

namespace {

class ReentrantOpeningPagePopupClient final
    : public GarbageCollected<ReentrantOpeningPagePopupClient>,
      public PagePopupClient {
 public:
  ReentrantOpeningPagePopupClient(ChromeClient& chrome_client,
                                  Element& owner,
                                  PagePopupClient* inner_client,
                                  bool self_close_after_reentrant_open = false)
      : chrome_client_(chrome_client),
        owner_(owner),
        inner_client_(inner_client),
        self_close_after_reentrant_open_(self_close_after_reentrant_open) {}

  void Trace(Visitor* visitor) const override {
    visitor->Trace(chrome_client_);
    visitor->Trace(owner_);
    visitor->Trace(inner_client_);
    PagePopupClient::Trace(visitor);
  }

  void WriteDocument(SegmentedBuffer& data) override {
    if (self_close_after_reentrant_open_) {
      PagePopupClient::AddLiteral(
          "<!DOCTYPE html><body><script>window.close();</script></body>", data);
    } else {
      PagePopupClient::AddLiteral("<!DOCTYPE html><body></body>", data);
    }
    if (inner_client_) {
      PagePopupClient* inner = inner_client_.Release();
      if (PagePopup* inner_popup = chrome_client_->OpenPagePopup(inner)) {
        inner_popup_handle_ = To<WebPagePopupImpl>(inner_popup)->GetHandle();
      }
    }
  }

  Element& OwnerElement() override { return *owner_; }
  ChromeClient& GetChromeClient() override { return *chrome_client_; }
  Locale& GetLocale() override { return Locale::DefaultLocale(); }
  void SetValueAndClosePopup(int, const String&, bool) override {}
  void SetValue(const String&) override {}
  void CancelPopup() override {}
  void DidClosePopup() override { ++did_close_count_; }

  WebPagePopup::Handle InnerPopupHandle() const { return inner_popup_handle_; }
  int DidCloseCount() const { return did_close_count_; }

 private:
  Member<ChromeClient> chrome_client_;
  Member<Element> owner_;
  Member<PagePopupClient> inner_client_;
  bool self_close_after_reentrant_open_ = false;
  WebPagePopup::Handle inner_popup_handle_;
  int did_close_count_ = 0;
};

}  // namespace

// Regression test for crbug.com/586391223: if a second popup is opened
// re-entrantly while the outer popup is still initializing, the outer
// OpenPagePopup() must close its popup cleanly without clobbering the inner
// popup or double-registering the popup mouse wheel listener.
TEST_F(InternalPopupMenuTest, OpenPopupReentrantlyDuringInitialization) {
  if (!RuntimeEnabledFeatures::PagePopupEnabled()) {
    return;
  }

  frame_test_helpers::WebViewHelper web_view_helper;
  WebViewImpl* web_view = web_view_helper.Initialize();
  web_view->MainFrameViewWidget()->Resize(gfx::Size(800, 600));

  WebURL base_url = url_test_helpers::ToKURL("http://example.com/");
  frame_test_helpers::LoadHTMLString(web_view->MainFrameImpl(), R"HTML(
    <!DOCTYPE html>
    <select id="sel">
      <option>1</option>
    </select>
  )HTML",
                                     base_url);

  Document& document =
      *web_view->MainFrameImpl()->GetDocument().Unwrap<Document>();
  document.View()->UpdateAllLifecyclePhasesForTest();

  auto* sel =
      To<HTMLSelectElement>(document.getElementById(AtomicString("sel")));
  ASSERT_TRUE(sel);

  for (bool self_close : {false, true}) {
    auto* inner_client = MakeGarbageCollected<InternalPopupMenu>(
        &web_view->GetChromeClient(), *sel);
    auto* outer_client = MakeGarbageCollected<ReentrantOpeningPagePopupClient>(
        web_view->GetChromeClient(), *sel, inner_client, self_close);

    WebPagePopupImpl* outer_popup = web_view->OpenPagePopup(outer_client);
    EXPECT_EQ(nullptr, outer_popup);
    EXPECT_EQ(1, outer_client->DidCloseCount());
    ASSERT_TRUE(web_view->GetPagePopup());
    EXPECT_EQ(outer_client->InnerPopupHandle(),
              web_view->GetPagePopup()->GetHandle());

    test::RunPendingTasks();
    EXPECT_EQ(nullptr, web_view->GetPagePopup());
  }
}

// The mousedown that opens a <select> popup suspends caret blinking, and the
// matching mouseup may go to the popup widget instead of the page. Closing the
// popup must resume caret blinking.
// See crbug.com/40899717.
TEST_F(InternalPopupMenuTest, ClosingPopupResumesCaretBlinking) {
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

  WebMouseEvent mouse_event(WebInputEvent::Type::kMouseDown,
                            WebInputEvent::kNoModifiers,
                            WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = WebMouseEvent::Button::kLeft;
  mouse_event.SetPositionInWidget(10, 10);
  mouse_event.SetPositionInScreen(10, 10);
  mouse_event.click_count = 1;
  web_view->MainFrameWidget()->HandleInputEvent(
      WebCoalescedInputEvent(mouse_event, {}));

  ASSERT_TRUE(select->PopupIsVisible());
  EXPECT_TRUE(document.GetFrame()->Selection().IsCaretBlinkingSuspended());

  WebPagePopupImpl* popup = web_view->GetPagePopup();
  ASSERT_TRUE(popup);
  popup->DidShowPopup();
  popup->ClosePopup();

  EXPECT_FALSE(select->PopupIsVisible());
  EXPECT_FALSE(document.GetFrame()->Selection().IsCaretBlinkingSuspended());
}

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace blink
