// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/clipboard/clipboard.h"

#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/clipboard/clipboard.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_blob.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_clipboard_read_options.h"
#include "third_party/blink/renderer/core/clipboard/system_clipboard.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard_item.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard_promise.h"
#include "third_party/blink/renderer/modules/clipboard/mock_clipboard_permission_service.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

using ::testing::WithArg;

// Helper class that validates ClipboardItem types match expected format list.
class ClipboardItemTypesValidator final
    : public ThenCallable<IDLSequence<ClipboardItem>,
                          ClipboardItemTypesValidator,
                          IDLBoolean> {
 public:
  explicit ClipboardItemTypesValidator(const Vector<String>& expected_types)
      : expected_types_(expected_types) {}

  bool React(ScriptState* script_state,
             HeapVector<Member<ClipboardItem>> clipboard_items) {
    if (clipboard_items.empty()) {
      return expected_types_.empty();
    }
    auto& clipboard_item = clipboard_items[0];
    Vector<String> available_types = clipboard_item->types();
    std::sort(available_types.begin(), available_types.end(),
              CodeUnitCompareLessThan);
    return available_types == expected_types_;
  }

 private:
  Vector<String> expected_types_;
};

class ClipboardItemGetType final
    : public ThenCallable<IDLSequence<ClipboardItem>,
                          ClipboardItemGetType,
                          IDLPromise<Blob>> {
 public:
  explicit ClipboardItemGetType(const String& expected_type)
      : expected_type_(expected_type) {}

  ScriptPromise<Blob> React(ScriptState* script_state,
                            HeapVector<Member<ClipboardItem>> clipboard_items) {
    if (clipboard_items.empty()) {
      return ScriptPromise<Blob>();
    }

    auto& clipboard_item = clipboard_items[0];

    ExceptionState exception_state(script_state->GetIsolate());

    // Call getType to trigger the underlying clipboard read
    return clipboard_item->getType(script_state, expected_type_,
                                   exception_state);
  }

 private:
  String expected_type_;
};

// This is a helper class which provides utility methods
// for testing the Async Clipboard API.
class ClipboardTest : public PageTestBase {
 public:
  void SetUp() override {
    PageTestBase::SetUp();
    // Clear PageTestBase's binder so we can install our own accessible mock.
    GetFrame().GetBrowserInterfaceBroker().SetBinderForTesting(
        mojom::blink::ClipboardHost::Name_, {});
    clipboard_provider_ =
        std::make_unique<PageTestBase::MockClipboardHostProvider>(
            GetFrame().GetBrowserInterfaceBroker());
  }

  void SetPageFocus(bool focused) {
    GetPage().GetFocusController().SetActive(focused);
    GetPage().GetFocusController().SetFocused(focused);
  }

  void BindMockPermissionService(ExecutionContext* executionContext) {
    executionContext->GetBrowserInterfaceBroker().SetBinderForTesting(
        mojom::blink::PermissionService::Name_,
        BindRepeating(&MockClipboardPermissionService::BindRequest,
                      Unretained(&permission_service_)));
  }

  void SetSecureOrigin(ExecutionContext* executionContext) {
    KURL page_url("https://example.com");
    scoped_refptr<SecurityOrigin> page_origin =
        SecurityOrigin::Create(page_url);
    executionContext->GetSecurityContext().SetSecurityOriginForTesting(nullptr);
    executionContext->GetSecurityContext().SetSecurityOrigin(page_origin);
  }

  void WritePlainTextToClipboard(const String& text) {
    GetFrame().GetSystemClipboard()->WritePlainText(text);
  }

  void WriteHtmlToClipboard(const String& html) {
    GetFrame().GetSystemClipboard()->WriteHTML(
        html, BlankUrl(), SystemClipboard::kCannotSmartReplace);
  }

 protected:
  MockClipboardPermissionService permission_service_;
  MockClipboardHost* mock_clipboard_host() {
    return clipboard_provider_->clipboard_host();
  }

 private:
  std::unique_ptr<PageTestBase::MockClipboardHostProvider> clipboard_provider_;
};

// Creates a ClipboardPromise for reading text from the clipboard and verifies
// that the promise resolves with the text provided to the MockSystemClipboard.
TEST_F(ClipboardTest, ClipboardPromiseReadText) {
  V8TestingScope scope;
  ExecutionContext* executionContext = GetFrame().DomWindow();
  String testing_string = "TestStringForClipboardTesting";
  WritePlainTextToClipboard(testing_string);

  // Async read clipboard API requires the clipboard read permission.
  EXPECT_CALL(permission_service_, RequestPermission)
      .WillOnce(WithArg<2>(
          [](mojom::blink::PermissionService::RequestPermissionCallback
                 callback) {
            std::move(callback).Run(mojom::blink::PermissionStatus::GRANTED);
          }));
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

  executionContext->GetBrowserInterfaceBroker().SetBinderForTesting(
      mojom::blink::PermissionService::Name_, {});
}

// Tests reading specific clipboard formats using ClipboardReadOptions.
// Verifies that only requested formats are returned when clipboard contains
// multiple formats.
TEST_F(ClipboardTest, SelectiveClipboardFormatRead) {
  V8TestingScope scope;
  ExecutionContext* executionContext = GetFrame().DomWindow();
  String testing_string = "TestStringForClipboardTesting";
  String html_to_paste = "<p>TestHtmlForClipboardTesting</p>";
  WritePlainTextToClipboard(testing_string);
  WriteHtmlToClipboard(html_to_paste);

  // Async read clipboard API requires the clipboard read permission.
  EXPECT_CALL(permission_service_, RequestPermission)
      .WillOnce(WithArg<2>(
          [](mojom::blink::PermissionService::RequestPermissionCallback
                 callback) {
            std::move(callback).Run(mojom::blink::PermissionStatus::GRANTED);
          }));
  BindMockPermissionService(executionContext);

  SetSecureOrigin(executionContext);
  SetPageFocus(true);
  // Create ClipboardReadOptions to filter for specific formats only.
  ClipboardReadOptions* options = ClipboardReadOptions::Create();
  Vector<String> requested_types;
  requested_types.emplace_back("text/plain");
  options->setTypes(requested_types);

  ScriptPromise<IDLSequence<ClipboardItem>> promise =
      ClipboardPromise::CreateForRead(executionContext, scope.GetScriptState(),
                                      options, scope.GetExceptionState());
  Vector<String> expected_types_in_result;
  expected_types_in_result.emplace_back("text/plain");
  // Validate that only plain text format is returned by the clipboard read.
  auto chained_promise = promise.Then(
      scope.GetScriptState(), MakeGarbageCollected<ClipboardItemTypesValidator>(
                                  expected_types_in_result));
  ScriptPromiseTester promise_tester(scope.GetScriptState(), chained_promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsFulfilled());
  String validation_result;
  promise_tester.Value().ToString(validation_result);
  EXPECT_EQ(validation_result, "true");
}

// Verifies that all formats are returned when no specific types are requested.
TEST_F(ClipboardTest, ReadAllClipboardFormats) {
  V8TestingScope scope;
  ExecutionContext* executionContext = GetFrame().DomWindow();
  String testing_string = "TestStringForClipboardTesting";
  String html_to_paste = "<p>TestHtmlForClipboardTesting</p>";
  WritePlainTextToClipboard(testing_string);
  WriteHtmlToClipboard(html_to_paste);

  // Async read clipboard API requires the clipboard read permission.
  EXPECT_CALL(permission_service_, RequestPermission)
      .WillOnce(WithArg<2>(
          [](mojom::blink::PermissionService::RequestPermissionCallback
                 callback) {
            std::move(callback).Run(mojom::blink::PermissionStatus::GRANTED);
          }));
  BindMockPermissionService(executionContext);

  SetSecureOrigin(executionContext);
  SetPageFocus(true);

  // Pass nullptr for options to read all available clipboard formats without
  // filtering.
  ScriptPromise<IDLSequence<ClipboardItem>> promise =
      ClipboardPromise::CreateForRead(executionContext, scope.GetScriptState(),
                                      nullptr, scope.GetExceptionState());
  Vector<String> expected_types_in_result;
  expected_types_in_result.emplace_back("text/html");
  expected_types_in_result.emplace_back("text/plain");
  auto chained_promise = promise.Then(
      scope.GetScriptState(), MakeGarbageCollected<ClipboardItemTypesValidator>(
                                  expected_types_in_result));
  ScriptPromiseTester promise_tester(scope.GetScriptState(), chained_promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsFulfilled());
  String validation_result;
  promise_tester.Value().ToString(validation_result);
  EXPECT_EQ(validation_result, "true");
}

// Test verifies that CreateForRead performs lazy loading by directly checking
// that data reading methods (ReadText, ReadHtml, etc.) are NOT called during
// clipboard.read(). Only ReadAvailableCustomAndStandardFormats should be
// called.
TEST_F(ClipboardTest, ReadOnlyMimeTypesInClipboardRead) {
  V8TestingScope scope;
  ExecutionContext* executionContext = GetFrame().DomWindow();
  String testing_string = "TestStringForClipboardTesting";
  String html_to_paste = "<p>TestHtmlForClipboardTesting</p>";

  // Write test data.
  WritePlainTextToClipboard(testing_string);
  WriteHtmlToClipboard(html_to_paste);

  // Mock permission service to grant clipboard access
  EXPECT_CALL(permission_service_, RequestPermission)
      .WillOnce(WithArg<2>(
          [](mojom::blink::PermissionService::RequestPermissionCallback
                 callback) {
            std::move(callback).Run(mojom::blink::PermissionStatus::GRANTED);
          }));
  BindMockPermissionService(executionContext);

  SetSecureOrigin(executionContext);
  SetPageFocus(true);

  // Execute CreateForRead - this should implement lazy loading
  ScriptPromise<IDLSequence<ClipboardItem>> promise =
      ClipboardPromise::CreateForRead(executionContext, scope.GetScriptState(),
                                      nullptr, scope.GetExceptionState());

  ScriptPromiseTester promise_tester(scope.GetScriptState(), promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsFulfilled());

  // Check that only type enumeration was called, not data reading
  // This proves that CreateForRead implements lazy loading correctly
  EXPECT_GT(mock_clipboard_host()->ReadAvailableFormatsCallCount(), 0);
  EXPECT_EQ(mock_clipboard_host()->ReadTextCallCount(), 0);
  EXPECT_EQ(mock_clipboard_host()->ReadHtmlCallCount(), 0);
}

TEST_F(ClipboardTest, ClipboardItemGetTypeTest) {
  V8TestingScope scope;
  ExecutionContext* executionContext = GetFrame().DomWindow();

  String testing_string = "TestStringForGetType";
  String html_to_paste = "<p>TestHtmlForGetType</p>";

  WritePlainTextToClipboard(testing_string);
  WriteHtmlToClipboard(html_to_paste);

  // Mock permission service
  EXPECT_CALL(permission_service_, RequestPermission)
      .WillOnce(WithArg<2>(
          [](mojom::blink::PermissionService::RequestPermissionCallback
                 callback) {
            std::move(callback).Run(mojom::blink::PermissionStatus::GRANTED);
          }));
  BindMockPermissionService(executionContext);

  SetSecureOrigin(executionContext);
  SetPageFocus(true);

  // Get clipboard items first
  ScriptPromise<IDLSequence<ClipboardItem>> promise =
      ClipboardPromise::CreateForRead(executionContext, scope.GetScriptState(),
                                      nullptr, scope.GetExceptionState());

  ScriptPromiseTester original_promise_tester(scope.GetScriptState(), promise);
  original_promise_tester.WaitUntilSettled();
  EXPECT_TRUE(original_promise_tester.IsFulfilled());

  // Verify that CreateForRead implemented lazy loading and didn't trigger
  // ReadText/ReadHtml yet
  EXPECT_EQ(mock_clipboard_host()->ReadAvailableFormatsCallCount(), 1);
  EXPECT_EQ(mock_clipboard_host()->ReadTextCallCount(), 0);
  EXPECT_EQ(mock_clipboard_host()->ReadHtmlCallCount(), 0);

  // Chain with ClipboardItemGetType to call getType()
  auto* get_type_helper =
      MakeGarbageCollected<ClipboardItemGetType>("text/plain");
  auto chained_promise = promise.Then(scope.GetScriptState(), get_type_helper);

  ScriptPromiseTester promise_tester(scope.GetScriptState(), chained_promise);
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsFulfilled())
      << "ClipboardItemGetType should succeed";

  // Verify that getType returned a blob with expected size
  ScriptValue value = promise_tester.Value();
  v8::Local<v8::Value> v8_value = value.V8Value();

  if (v8_value->IsPromise()) {
    ScriptPromise<Blob> inner_src_promise =
        ScriptPromise<Blob>::FromV8Value(scope.GetScriptState(), v8_value);
    ScriptPromiseTester inner_tester(scope.GetScriptState(), inner_src_promise);
    inner_tester.WaitUntilSettled();
    value = inner_tester.Value();
    v8_value = value.V8Value();
  }

  Blob* blob = NativeValueTraits<Blob>::NativeValue(
      scope.GetIsolate(), v8_value, scope.GetExceptionState());
  ASSERT_TRUE(blob) << "Blob should not be null";
  EXPECT_EQ(blob->size(), testing_string.Utf8().size())
      << "Blob size should match expected content size";

  // Verify first getType() triggered a single ReadText call.
  EXPECT_EQ(mock_clipboard_host()->ReadTextCallCount(), 1);
  EXPECT_EQ(mock_clipboard_host()->ReadHtmlCallCount(), 0);

  // Call getType() again on the same ClipboardItem type and verify it uses the
  // cached promise/value rather than reading from OS clipboard again.
  auto* second_get_type_helper =
      MakeGarbageCollected<ClipboardItemGetType>("text/plain");
  auto second_chained_promise =
      promise.Then(scope.GetScriptState(), second_get_type_helper);

  ScriptPromiseTester second_promise_tester(scope.GetScriptState(),
                                            second_chained_promise);
  second_promise_tester.WaitUntilSettled();
  EXPECT_TRUE(second_promise_tester.IsFulfilled())
      << "Second ClipboardItemGetType should succeed";

  // Cached getType() should not trigger another ReadText call.
  EXPECT_EQ(mock_clipboard_host()->ReadTextCallCount(), 1);
  EXPECT_EQ(mock_clipboard_host()->ReadHtmlCallCount(), 0);
}

}  // namespace blink
