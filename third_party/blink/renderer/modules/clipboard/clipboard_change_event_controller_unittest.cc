// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/clipboard/clipboard_change_event_controller.h"

#include "base/functional/bind.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard_test_utils.h"
#include "third_party/blink/renderer/modules/clipboard/mock_clipboard_permission_service.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class ClipboardChangeEventTest : public ClipboardTestBase {
 public:
  void BindMockPermissionService(ExecutionContext* execution_context) {
    execution_context->GetBrowserInterfaceBroker().SetBinderForTesting(
        mojom::blink::PermissionService::Name_,
        base::BindRepeating(&MockClipboardPermissionService::BindRequest,
                            base::Unretained(&permission_service_)));
  }

 protected:
  MockClipboardPermissionService permission_service_;
};

TEST_F(ClipboardChangeEventTest, ClipboardChangeEventFiresWhenFocused) {
  ExecutionContext* execution_context = GetFrame().DomWindow();
  // Set the yet-unset optional in SystemClipboard as we will be triggering
  // events artificially.
  GetFrame().GetSystemClipboard()->OnClipboardDataChanged({"text/plain"}, 1);

  // Clipboardchange event requires a secure origin and page in focus to work.
  SetSecureOrigin(execution_context);
  SetPageFocus(true);

  auto* clipboard_change_event_handler =
      MakeGarbageCollected<EventCountingListener>();
  GetDocument().addEventListener(event_type_names::kClipboardchange,
                                 clipboard_change_event_handler, false);
  auto* clipboard_change_event_controller =
      MakeGarbageCollected<ClipboardChangeEventController>(
          *GetFrame().DomWindow()->navigator(), &GetDocument());

  EXPECT_EQ(clipboard_change_event_handler->Count(), 0);

  // Set sticky user activation to ensure the event fires immediately
  // Use the same frame context that the controller will use
  LocalDOMWindow* local_dom_window = To<LocalDOMWindow>(execution_context);
  LocalFrame* test_frame = local_dom_window->GetFrame();
  test_frame->SetStickyUserActivationState();

  // Simulate a clipboard change event from the system clipboard.
  clipboard_change_event_controller->DidUpdateData();

  // Wait for any internal callbacks to run.
  test::RunPendingTasks();

  // Expect a single clipboardchange event to be fired.
  EXPECT_EQ(clipboard_change_event_handler->Count(), 1);

  // Clean up the event listener
  GetDocument().removeEventListener(event_type_names::kClipboardchange,
                                    clipboard_change_event_handler, false);
}

TEST_F(ClipboardChangeEventTest, ClipboardChangeEventNotFiredWhenNotFocused) {
  ExecutionContext* execution_context = GetFrame().DomWindow();
  // Set the yet-unset optional in SystemClipboard as we will be triggering
  // events artificially.
  GetFrame().GetSystemClipboard()->OnClipboardDataChanged({"text/plain"}, 1);

  SetSecureOrigin(execution_context);
  SetPageFocus(false);

  auto* clipboard_change_event_handler =
      MakeGarbageCollected<EventCountingListener>();
  GetDocument().addEventListener(event_type_names::kClipboardchange,
                                 clipboard_change_event_handler, false);
  auto* clipboard_change_event_controller =
      MakeGarbageCollected<ClipboardChangeEventController>(
          *GetFrame().DomWindow()->navigator(), &GetDocument());

  EXPECT_EQ(clipboard_change_event_handler->Count(), 0);

  // Simulate a clipboard change event from the system clipboard.
  clipboard_change_event_controller->DidUpdateData();

  // Wait for any internal callbacks to run.
  test::RunPendingTasks();

  // Expect no clipboardchange event to be fired since the page is not focused.
  EXPECT_EQ(clipboard_change_event_handler->Count(), 0);

  // Simulate more clipboard updates
  clipboard_change_event_controller->DidUpdateData();
  clipboard_change_event_controller->DidUpdateData();

  // Set sticky activation on the same frame the controller will use
  GetDocument().domWindow()->GetFrame()->SetStickyUserActivationState();
  SetPageFocus(true);
  test::RunPendingTasks();

  // Expect a single clipboardchange event to be fired
  // now that the page is focused and has user activation.
  EXPECT_EQ(clipboard_change_event_handler->Count(), 1);

  // Clean up the event listener
  GetDocument().removeEventListener(event_type_names::kClipboardchange,
                                    clipboard_change_event_handler, false);
}

TEST_F(ClipboardChangeEventTest,
       ClipboardChangeEventFiresWithStickyActivation) {
  ExecutionContext* execution_context = GetFrame().DomWindow();
  // Set the yet-unset optional in SystemClipboard as we will be triggering
  // events artificially.
  GetFrame().GetSystemClipboard()->OnClipboardDataChanged({"text/plain"}, 1);

  // Clipboardchange event requires a secure origin and page in focus to work.
  SetSecureOrigin(execution_context);
  SetPageFocus(true);

  auto* clipboard_change_event_handler =
      MakeGarbageCollected<EventCountingListener>();
  GetDocument().addEventListener(event_type_names::kClipboardchange,
                                 clipboard_change_event_handler, false);
  auto* clipboard_change_event_controller =
      MakeGarbageCollected<ClipboardChangeEventController>(
          *GetFrame().DomWindow()->navigator(), &GetDocument());

  EXPECT_EQ(clipboard_change_event_handler->Count(), 0);

  // Simulate sticky user activation
  GetFrame().SetStickyUserActivationState();

  // Simulate a clipboard change event from the system clipboard.
  clipboard_change_event_controller->DidUpdateData();

  // Wait for any internal callbacks to run.
  test::RunPendingTasks();

  // Expect a single clipboardchange event to be fired due to sticky activation.
  EXPECT_EQ(clipboard_change_event_handler->Count(), 1);

  // Clean up the event listener
  GetDocument().removeEventListener(event_type_names::kClipboardchange,
                                    clipboard_change_event_handler, false);
}

TEST_F(ClipboardChangeEventTest,
       ClipboardChangeEventNotFiredWithoutStickyActivationOrPermission) {
  ExecutionContext* execution_context = GetFrame().DomWindow();
  // Set the yet-unset optional in SystemClipboard as we will be triggering
  // events artificially.
  GetFrame().GetSystemClipboard()->OnClipboardDataChanged({"text/plain"}, 1);

  SetSecureOrigin(execution_context);
  SetPageFocus(true);

  auto* clipboard_change_event_handler =
      MakeGarbageCollected<EventCountingListener>();
  GetDocument().addEventListener(event_type_names::kClipboardchange,
                                 clipboard_change_event_handler, false);

  // Mock permission service to deny clipboard-read permission
  BindMockPermissionService(execution_context);
  EXPECT_CALL(permission_service_, HasPermission(testing::_, testing::_))
      .WillOnce(
          [](mojom::blink::PermissionDescriptorPtr,
             MockClipboardPermissionService::HasPermissionCallback callback) {
            std::move(callback).Run(mojom::blink::PermissionStatus::DENIED);
          });

  auto* clipboard_change_event_controller =
      MakeGarbageCollected<ClipboardChangeEventController>(
          *GetFrame().DomWindow()->navigator(), &GetDocument());

  EXPECT_EQ(clipboard_change_event_handler->Count(), 0);

  // No sticky activation and permission will be denied
  EXPECT_FALSE(GetFrame().HasStickyUserActivation());

  // Simulate a clipboard change event from the system clipboard.
  clipboard_change_event_controller->DidUpdateData();

  // Wait for any internal callbacks to run.
  test::RunPendingTasks();

  // Expect no clipboardchange event to be fired since there's no sticky
  // activation and clipboard-read permission is denied.
  EXPECT_EQ(clipboard_change_event_handler->Count(), 0);

  // Clean up the event listener
  GetDocument().removeEventListener(event_type_names::kClipboardchange,
                                    clipboard_change_event_handler, false);
}

// Note: Test for clipboard-read permission granted case is omitted due to
// mock permission service binding issues in the test environment that cause
// dangling pointer crashes. The permission denial path is successfully tested
// by ClipboardChangeEventNotFiredWithoutStickyActivationOrPermission.

TEST_F(ClipboardChangeEventTest,
       StickyActivationTakesPrecedenceOverPermissionCheck) {
  ExecutionContext* execution_context = GetFrame().DomWindow();
  // Set the yet-unset optional in SystemClipboard as we will be triggering
  // events artificially.
  GetFrame().GetSystemClipboard()->OnClipboardDataChanged({"text/plain"}, 1);

  SetSecureOrigin(execution_context);
  SetPageFocus(true);

  auto* clipboard_change_event_handler =
      MakeGarbageCollected<EventCountingListener>();
  GetDocument().addEventListener(event_type_names::kClipboardchange,
                                 clipboard_change_event_handler, false);

  auto* clipboard_change_event_controller =
      MakeGarbageCollected<ClipboardChangeEventController>(
          *GetFrame().DomWindow()->navigator(), &GetDocument());

  EXPECT_EQ(clipboard_change_event_handler->Count(), 0);

  // Set sticky activation - this should bypass permission check entirely
  GetFrame().SetStickyUserActivationState();
  EXPECT_TRUE(GetFrame().HasStickyUserActivation());

  // Simulate a clipboard change event from the system clipboard.
  clipboard_change_event_controller->DidUpdateData();

  // Wait for any internal callbacks to run.
  test::RunPendingTasks();

  // Expect a single clipboardchange event to be fired due to sticky activation,
  // without any permission check. The presence of sticky activation should
  // make the permission check unnecessary.
  EXPECT_EQ(clipboard_change_event_handler->Count(), 1);

  // Clean up the event listener
  GetDocument().removeEventListener(event_type_names::kClipboardchange,
                                    clipboard_change_event_handler, false);
}

}  // namespace blink
