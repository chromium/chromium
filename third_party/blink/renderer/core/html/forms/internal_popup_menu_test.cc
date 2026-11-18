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
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/web/web_autofill_client.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/renderer/core/css/media_feature_names.h"
#include "third_party/blink/renderer/core/css/media_feature_overrides.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/exported/web_page_popup_impl.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/page_popup_controller.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
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
  static_cast<WebPagePopup*>(popup)->SetScreenRects(gfx::Rect(0, 0, 200, 200), gfx::Rect(0, 0, 200, 200));
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
      static_cast<WebPagePopup*>(popup)->ProcessInputEventSynchronouslyForTesting(
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
      static_cast<WebPagePopup*>(popup)->ProcessInputEventSynchronouslyForTesting(
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
      static_cast<WebPagePopup*>(popup)->ProcessInputEventSynchronouslyForTesting(
          WebCoalescedInputEvent(gesture_event, {}));
      break;
    }
    case PopupInputDispatchKind::kPopupKeyDown: {
      WebKeyboardEvent key_event(WebInputEvent::Type::kRawKeyDown,
                                 WebInputEvent::kNoModifiers,
                                 WebInputEvent::GetStaticTimeStampForTests());
      key_event.windows_key_code = VKEY_DOWN;
      static_cast<WebPagePopup*>(popup)->ProcessInputEventSynchronouslyForTesting(
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
  static_cast<WebPagePopup*>(popup)->SetScreenRects(gfx::Rect(0, 0, 200, 200), gfx::Rect(0, 0, 200, 200));
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
  static_cast<WebPagePopup*>(popup)->ProcessInputEventSynchronouslyForTesting(
      WebCoalescedInputEvent(mouse_event, {}));
  EXPECT_TRUE(popup_frame->GetEventHandler().MousePressed());

  WebKeyboardEvent key_event(WebInputEvent::Type::kRawKeyDown,
                             WebInputEvent::kNoModifiers,
                             WebInputEvent::GetStaticTimeStampForTests());
  key_event.windows_key_code = VKEY_SHIFT;
  static_cast<WebPagePopup*>(popup)->ProcessInputEventSynchronouslyForTesting(
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

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace blink
