// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/clipboard/clipboard.h"

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/clipboard/system_clipboard.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard_promise.h"
#include "third_party/blink/renderer/modules/clipboard/mock_clipboard_permission_service.h"

namespace blink {

using ::testing::Invoke;
using ::testing::WithArg;

// This is a helper class which provides utility methods
// for testing the Async Clipboard API.
class ClipboardTest : public PageTestBase {
 public:
  void SetPageFocus(bool focused) {
    GetPage().GetFocusController().SetActive(focused);
    GetPage().GetFocusController().SetFocused(focused);
  }

  void BindMockPermissionService(ExecutionContext* executionContext) {
    executionContext->GetBrowserInterfaceBroker().SetBinderForTesting(
        mojom::blink::PermissionService::Name_,
        WTF::BindRepeating(&MockClipboardPermissionService::BindRequest,
                           WTF::Unretained(&permission_service_)));
  }

  void SetSecureOrigin(ExecutionContext* executionContext) {
    KURL page_url("https://example.com");
    scoped_refptr<SecurityOrigin> page_origin =
        SecurityOrigin::Create(page_url);
    executionContext->GetSecurityContext().SetSecurityOriginForTesting(nullptr);
    executionContext->GetSecurityContext().SetSecurityOrigin(page_origin);
  }

  void WritePlainTextToClipboard(const String& text, V8TestingScope& scope) {
    scope.GetFrame().GetSystemClipboard()->WritePlainText(text);
  }

 protected:
  MockClipboardPermissionService permission_service_;
};

// Creates a ClipboardPromise for reading text from the clipboard and verifies
// that the promise resolves with the text provided to the MockSystemClipboard.
TEST_F(ClipboardTest, ClipboardPromiseReadText) {
  V8TestingScope scope;
  ExecutionContext* executionContext = GetFrame().DomWindow();
  String testing_string = "TestStringForClipboardTesting";
  WritePlainTextToClipboard(testing_string, scope);

  // Async read clipboard API requires the clipboard read permission.
  EXPECT_CALL(permission_service_, RequestPermission)
      .WillOnce(WithArg<2>(
          Invoke([](mojom::blink::PermissionService::RequestPermissionCallback
                        callback) {
            std::move(callback).Run(mojom::blink::PermissionStatus::GRANTED);
          })));
  BindMockPermissionService(executionContext);

  // Async clipboard API requires a secure origin and page in focus to work.
  SetSecureOrigin(executionContext);
  SetPageFocus(true);

  ScriptPromise<IDLString> promise = ClipboardPromise::CreateForReadText(
      executionContext, scope.GetScriptState(), scope.GetExceptionState());
  ScriptPromiseTester promise_tester(scope.GetScriptState(), promise);
  promise_tester.WaitUntilSettled();  // Runs a nested event loop.
  EXPECT_TRUE(promise_tester.IsFulfilled());
  String promise_returned_string;
  promise_tester.Value().ToString(promise_returned_string);
  EXPECT_EQ(promise_returned_string, testing_string);
}

}  // namespace blink
