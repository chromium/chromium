// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/gamepad/navigator_gamepad.h"

#include <memory>
#include <utility>

#include "base/memory/read_only_shared_memory_region.h"
#include "device/gamepad/public/cpp/gamepads.h"
#include "device/gamepad/public/mojom/gamepad.mojom-blink.h"
#include "device/gamepad/public/mojom/gamepad_hardware_buffer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/modules/gamepad/gamepad.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

using ::testing::_;
using ::testing::Each;
using ::testing::IsNull;
using ::testing::Mock;
using ::testing::NiceMock;

Gamepad* CreateGamepadWithButtonDown() {
  base::TimeTicks dummy_time_origin =
      base::TimeTicks() + base::Microseconds(1000);
  base::TimeTicks dummy_time_floor =
      base::TimeTicks() + base::Microseconds(2000);
  auto* gamepad = MakeGarbageCollected<Gamepad>(nullptr, 0, dummy_time_origin,
                                                dummy_time_floor);
  double axes[1] = {0.0};
  device::GamepadButton buttons[1] = {{true, true, 1.0}};
  gamepad->SetId("gamepad");
  gamepad->SetAxes(axes);
  gamepad->SetButtons(buttons);
  gamepad->SetConnected(true);
  return gamepad;
}

HeapVector<Member<Gamepad>> CreateGamepadListWithButtonDown() {
  HeapVector<Member<Gamepad>> list(device::Gamepads::kItemsLengthCap);
  list[0] = CreateGamepadWithButtonDown();
  return list;
}

class MockGamepadMonitor : public device::mojom::blink::GamepadMonitor {
 public:
  MockGamepadMonitor() {
    ON_CALL(*this, GamepadStartPolling(_))
        .WillByDefault([this](GamepadStartPollingCallback callback) {
          base::MappedReadOnlyRegion mapped_region =
              base::ReadOnlySharedMemoryRegion::Create(
                  sizeof(device::GamepadHardwareBuffer));
          if (!mapped_region.IsValid()) {
            receiver_.reset();
          }
          ASSERT_TRUE(mapped_region.IsValid());
          std::move(callback).Run(std::move(mapped_region.region));
        });
    ON_CALL(*this, GamepadStopPolling(_))
        .WillByDefault([](GamepadStopPollingCallback callback) {
          std::move(callback).Run();
        });
  }
  ~MockGamepadMonitor() override = default;

  void Bind(mojo::ScopedMessagePipeHandle handle) {
    receiver_.reset();
    receiver_.Bind(mojo::PendingReceiver<device::mojom::blink::GamepadMonitor>(
        std::move(handle)));
  }

  MOCK_METHOD(void,
              GamepadStartPolling,
              (GamepadStartPollingCallback callback),
              (override));
  MOCK_METHOD(void,
              GamepadStopPolling,
              (GamepadStopPollingCallback callback),
              (override));
  MOCK_METHOD(
      void,
      SetObserver,
      (mojo::PendingRemote<device::mojom::blink::GamepadObserver> observer),
      (override));

 private:
  mojo::Receiver<device::mojom::blink::GamepadMonitor> receiver_{this};
};

class ScopedMockGamepadMonitor {
  STACK_ALLOCATED();

 public:
  ScopedMockGamepadMonitor(BrowserInterfaceBrokerProxy& broker,
                           MockGamepadMonitor* monitor)
      : broker_(broker) {
    broker_.SetBinderForTesting(
        device::mojom::blink::GamepadMonitor::Name_,
        BindRepeating(&MockGamepadMonitor::Bind, Unretained(monitor)));
  }
  ~ScopedMockGamepadMonitor() {
    broker_.SetBinderForTesting(device::mojom::blink::GamepadMonitor::Name_,
                                {});
  }

 private:
  BrowserInterfaceBrokerProxy& broker_;
};

class DummyNativeEventListener final : public NativeEventListener {
 public:
  void Invoke(ExecutionContext*, Event*) override {}
};

}  // namespace

class NavigatorGamepadTest : public testing::Test {
 public:
  NavigatorGamepadTest() = default;

 protected:
  test::TaskEnvironment task_environment_;
};

TEST_F(NavigatorGamepadTest, UnfocusedPageDoesNotGainUserActivation) {
  auto page_holder = std::make_unique<DummyPageHolder>();
  LocalFrame& frame = page_holder->GetFrame();
  Page& page = page_holder->GetPage();

  EXPECT_TRUE(page.IsPageVisible());
  page.GetFocusController().SetActive(true);
  page.GetFocusController().SetFocused(true);
  page.GetFocusController().SetFocused(false);
  EXPECT_FALSE(page.GetFocusController().IsFocused());
  EXPECT_FALSE(frame.GetDocument()->hasFocus());

  Navigator* navigator = frame.DomWindow()->navigator();
  NavigatorGamepad& navigator_gamepad = NavigatorGamepad::From(*navigator);
  navigator_gamepad.SetGamepadsForTesting(CreateGamepadListWithButtonDown());

  EXPECT_FALSE(LocalFrame::HasTransientUserActivation(&frame));
  EXPECT_FALSE(frame.HasStickyUserActivation());

  navigator_gamepad.Gamepads();

  EXPECT_FALSE(LocalFrame::HasTransientUserActivation(&frame));
  EXPECT_FALSE(frame.HasStickyUserActivation());
}

TEST_F(NavigatorGamepadTest, FocusedPageGainsUserActivation) {
  auto page_holder = std::make_unique<DummyPageHolder>();
  LocalFrame& frame = page_holder->GetFrame();
  Page& page = page_holder->GetPage();

  EXPECT_TRUE(page.IsPageVisible());
  page.GetFocusController().SetActive(true);
  page.GetFocusController().SetFocused(true);
  EXPECT_TRUE(page.GetFocusController().IsFocused());
  EXPECT_TRUE(frame.GetDocument()->hasFocus());

  Navigator* navigator = frame.DomWindow()->navigator();
  NavigatorGamepad& navigator_gamepad = NavigatorGamepad::From(*navigator);
  navigator_gamepad.SetGamepadsForTesting(CreateGamepadListWithButtonDown());

  EXPECT_FALSE(LocalFrame::HasTransientUserActivation(&frame));
  EXPECT_FALSE(frame.HasStickyUserActivation());

  navigator_gamepad.Gamepads();

  EXPECT_TRUE(LocalFrame::HasTransientUserActivation(&frame));
  EXPECT_TRUE(frame.HasStickyUserActivation());
}

TEST_F(NavigatorGamepadTest, SubframeInFocusedPageGainsUserActivation) {
  auto page_holder = std::make_unique<DummyPageHolder>(
      gfx::Size(800, 600), /*chrome_client=*/nullptr,
      MakeGarbageCollected<SingleChildLocalFrameClient>());
  LocalFrame& main_frame = page_holder->GetFrame();
  Page& page = page_holder->GetPage();

  page.GetFocusController().SetActive(true);
  page.GetFocusController().SetFocused(true);
  EXPECT_TRUE(page.GetFocusController().IsFocused());

  page_holder->GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<iframe></iframe>");
  auto* iframe =
      To<HTMLIFrameElement>(page_holder->GetDocument().body()->firstChild());
  ASSERT_TRUE(iframe);
  LocalFrame* child_frame = To<LocalFrame>(iframe->ContentFrame());
  ASSERT_TRUE(child_frame);

  EXPECT_TRUE(main_frame.GetDocument()->hasFocus());
  EXPECT_FALSE(child_frame->GetDocument()->hasFocus());

  Navigator* child_navigator = child_frame->DomWindow()->navigator();
  NavigatorGamepad& child_navigator_gamepad =
      NavigatorGamepad::From(*child_navigator);
  child_navigator_gamepad.SetGamepadsForTesting(
      CreateGamepadListWithButtonDown());

  EXPECT_FALSE(LocalFrame::HasTransientUserActivation(child_frame));
  EXPECT_FALSE(child_frame->HasStickyUserActivation());

  child_navigator_gamepad.Gamepads();

  EXPECT_TRUE(LocalFrame::HasTransientUserActivation(child_frame));
  EXPECT_TRUE(child_frame->HasStickyUserActivation());
}

// Regression test for https://crbug.com/578297821.
TEST_F(NavigatorGamepadTest,
       HiddenPageGetGamepadsDoesNotStartOrRestartPolling) {
  NiceMock<MockGamepadMonitor> mock_monitor;
  auto page_holder = std::make_unique<DummyPageHolder>();
  LocalFrame& frame = page_holder->GetFrame();
  Page& page = page_holder->GetPage();

  ScopedMockGamepadMonitor scoped_monitor(frame.GetBrowserInterfaceBroker(),
                                          &mock_monitor);

  page.SetVisibilityState(mojom::blink::PageVisibilityState::kHidden,
                          /*is_initial_state=*/true);

  Navigator* navigator = frame.DomWindow()->navigator();
  NavigatorGamepad& navigator_gamepad = NavigatorGamepad::From(*navigator);

  EXPECT_CALL(mock_monitor, GamepadStartPolling(_)).Times(0);
  HeapVector<Member<Gamepad>> gamepads = navigator_gamepad.Gamepads();
  EXPECT_EQ(gamepads.size(), device::Gamepads::kItemsLengthCap);
  Mock::VerifyAndClearExpectations(&mock_monitor);

  EXPECT_CALL(mock_monitor, GamepadStartPolling(_)).Times(1);
  page.SetVisibilityState(mojom::blink::PageVisibilityState::kVisible,
                          /*is_initial_state=*/false);
  Mock::VerifyAndClearExpectations(&mock_monitor);

  EXPECT_CALL(mock_monitor, GamepadStopPolling(_)).Times(1);
  page.SetVisibilityState(mojom::blink::PageVisibilityState::kHidden,
                          /*is_initial_state=*/false);
  Mock::VerifyAndClearExpectations(&mock_monitor);

  EXPECT_CALL(mock_monitor, GamepadStartPolling(_)).Times(0);
  gamepads = navigator_gamepad.Gamepads();
  EXPECT_EQ(gamepads.size(), device::Gamepads::kItemsLengthCap);
  Mock::VerifyAndClearExpectations(&mock_monitor);

  EXPECT_CALL(mock_monitor, GamepadStartPolling(_)).Times(1);
  page.SetVisibilityState(mojom::blink::PageVisibilityState::kVisible,
                          /*is_initial_state=*/false);
  Mock::VerifyAndClearExpectations(&mock_monitor);
}

// Regression test for https://crbug.com/581016068.
TEST_F(NavigatorGamepadTest, PrerenderingPageGetGamepadsStartsPolling) {
  NiceMock<MockGamepadMonitor> mock_monitor;
  auto page_holder = std::make_unique<DummyPageHolder>();
  LocalFrame& frame = page_holder->GetFrame();
  Page& page = page_holder->GetPage();

  ScopedMockGamepadMonitor scoped_monitor(frame.GetBrowserInterfaceBroker(),
                                          &mock_monitor);

  page.SetVisibilityState(mojom::blink::PageVisibilityState::kHidden,
                          /*is_initial_state=*/true);
  page.SetIsPrerendering(true);

  Navigator* navigator = frame.DomWindow()->navigator();
  NavigatorGamepad& navigator_gamepad = NavigatorGamepad::From(*navigator);

  EXPECT_CALL(mock_monitor, GamepadStartPolling(_)).Times(1);
  HeapVector<Member<Gamepad>> gamepads = navigator_gamepad.Gamepads();
  EXPECT_EQ(gamepads.size(), device::Gamepads::kItemsLengthCap);
  EXPECT_THAT(gamepads, Each(IsNull()));
  Mock::VerifyAndClearExpectations(&mock_monitor);
}

// Verifies detached subframe operations do not crash or start polling
// (https://crbug.com/578297821).
TEST_F(NavigatorGamepadTest,
       DetachedSubframeOperationsDoNotCrashOrStartPolling) {
  NiceMock<MockGamepadMonitor> child_mock_monitor;
  auto page_holder = std::make_unique<DummyPageHolder>(
      gfx::Size(800, 600), /*chrome_client=*/nullptr,
      MakeGarbageCollected<SingleChildLocalFrameClient>());
  Page& page = page_holder->GetPage();

  page_holder->GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<iframe></iframe>");
  auto* iframe =
      To<HTMLIFrameElement>(page_holder->GetDocument().body()->firstChild());
  ASSERT_TRUE(iframe);
  auto* child_frame = To<LocalFrame>(iframe->ContentFrame());
  ASSERT_TRUE(child_frame);

  ScopedMockGamepadMonitor scoped_monitor(
      child_frame->GetBrowserInterfaceBroker(), &child_mock_monitor);

  LocalDOMWindow* child_window = child_frame->DomWindow();
  Navigator* child_navigator = child_window->navigator();
  NavigatorGamepad& child_navigator_gamepad =
      NavigatorGamepad::From(*child_navigator);

  iframe->remove();
  EXPECT_FALSE(child_window->GetFrame());

  EXPECT_CALL(child_mock_monitor, GamepadStartPolling(_)).Times(0);

  HeapVector<Member<Gamepad>> gamepads = child_navigator_gamepad.Gamepads();
  EXPECT_EQ(gamepads.size(), device::Gamepads::kItemsLengthCap);

  auto* listener = MakeGarbageCollected<DummyNativeEventListener>();
  child_window->addEventListener(event_type_names::kGamepadconnected, listener);

  page.SetVisibilityState(mojom::blink::PageVisibilityState::kHidden,
                          /*is_initial_state=*/false);
  page.SetVisibilityState(mojom::blink::PageVisibilityState::kVisible,
                          /*is_initial_state=*/false);

  Mock::VerifyAndClearExpectations(&child_mock_monitor);
}

}  // namespace blink
