// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/clipboard/clipboard.h"

#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/clipboard/clipboard.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-blink.h"
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
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
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
      .WillOnce(WithArg<1>(
          [](mojom::blink::PermissionService::RequestPermissionCallback
                 callback) {
            std::move(callback).Run(
                mojom::blink::PermissionStatusWithDetails::New(
                    mojom::blink::PermissionStatus::GRANTED, nullptr));
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
      .WillOnce(WithArg<1>(
          [](mojom::blink::PermissionService::RequestPermissionCallback
                 callback) {
            std::move(callback).Run(
                mojom::blink::PermissionStatusWithDetails::New(
                    mojom::blink::PermissionStatus::GRANTED, nullptr));
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
      .WillOnce(WithArg<1>(
          [](mojom::blink::PermissionService::RequestPermissionCallback
                 callback) {
            std::move(callback).Run(
                mojom::blink::PermissionStatusWithDetails::New(
                    mojom::blink::PermissionStatus::GRANTED, nullptr));
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
      .WillOnce(WithArg<1>(
          [](mojom::blink::PermissionService::RequestPermissionCallback
                 callback) {
            std::move(callback).Run(
                mojom::blink::PermissionStatusWithDetails::New(
                    mojom::blink::PermissionStatus::GRANTED, nullptr));
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
      .WillOnce(WithArg<1>(
          [](mojom::blink::PermissionService::RequestPermissionCallback
                 callback) {
            std::move(callback).Run(
                mojom::blink::PermissionStatusWithDetails::New(
                    mojom::blink::PermissionStatus::GRANTED, nullptr));
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

// Tests that Blink.Clipboard.LazyRead.NullBlobResolved boolean histogram is
// recorded when ResolveFormatData() resolves with a valid blob.
TEST_F(ClipboardTest, LazyReadNullBlobResolved_Histogram) {
  base::HistogramTester histogram_tester;
  V8TestingScope scope;
  ExecutionContext* executionContext = GetFrame().DomWindow();

  String testing_string = "TestStringForNullBlob";
  WritePlainTextToClipboard(testing_string);

  EXPECT_CALL(permission_service_, RequestPermission)
      .WillOnce(WithArg<1>(
          [](mojom::blink::PermissionService::RequestPermissionCallback
                 callback) {
            std::move(callback).Run(
                mojom::blink::PermissionStatusWithDetails::New(
                    mojom::blink::PermissionStatus::GRANTED, nullptr));
          }));
  BindMockPermissionService(executionContext);
  SetSecureOrigin(executionContext);
  SetPageFocus(true);

  // Read clipboard items.
  ScriptPromise<IDLSequence<ClipboardItem>> promise =
      ClipboardPromise::CreateForRead(executionContext, scope.GetScriptState(),
                                      nullptr, scope.GetExceptionState());
  ScriptPromiseTester read_tester(scope.GetScriptState(), promise);
  read_tester.WaitUntilSettled();
  ASSERT_TRUE(read_tester.IsFulfilled());

  // Fetch text/plain — should succeed and record false (not null).
  auto* get_type_helper =
      MakeGarbageCollected<ClipboardItemGetType>("text/plain");
  auto chained_promise = promise.Then(scope.GetScriptState(), get_type_helper);
  ScriptPromiseTester get_type_tester(scope.GetScriptState(), chained_promise);
  get_type_tester.WaitUntilSettled();
  EXPECT_TRUE(get_type_tester.IsFulfilled());

  // Histogram should record false (valid blob resolved).
  histogram_tester.ExpectUniqueSample(
      "Blink.Clipboard.LazyRead.NullBlobResolved", false, 1);
}

// Tests that Blink.Clipboard.LazyRead.FormatsNeverRead histogram is
// recorded when a lazy ClipboardItem's SingleSampleMetric is destroyed.
TEST_F(ClipboardTest, LazyReadFormatsNeverRead_Histogram) {
  base::HistogramTester histogram_tester;
  V8TestingScope scope;
  ExecutionContext* executionContext = GetFrame().DomWindow();

  // Write both text and html so the ClipboardItem has 2 MIME types.
  String testing_string = "TestStringForHistogram";
  String html_to_paste = "<p>TestHtmlForHistogram</p>";
  WritePlainTextToClipboard(testing_string);
  WriteHtmlToClipboard(html_to_paste);

  EXPECT_CALL(permission_service_, RequestPermission)
      .WillOnce(WithArg<1>(
          [](mojom::blink::PermissionService::RequestPermissionCallback
                 callback) {
            std::move(callback).Run(
                mojom::blink::PermissionStatusWithDetails::New(
                    mojom::blink::PermissionStatus::GRANTED, nullptr));
          }));
  BindMockPermissionService(executionContext);
  SetSecureOrigin(executionContext);
  SetPageFocus(true);

  // Read clipboard items — creates lazy ClipboardItem with 2 MIME types.
  ScriptPromise<IDLSequence<ClipboardItem>> promise =
      ClipboardPromise::CreateForRead(executionContext, scope.GetScriptState(),
                                      nullptr, scope.GetExceptionState());
  ScriptPromiseTester read_tester(scope.GetScriptState(), promise);
  read_tester.WaitUntilSettled();
  ASSERT_TRUE(read_tester.IsFulfilled());

  // Only fetch one of the two types.
  auto* get_type_helper =
      MakeGarbageCollected<ClipboardItemGetType>("text/plain");
  auto chained_promise = promise.Then(scope.GetScriptState(), get_type_helper);
  ScriptPromiseTester get_type_tester(scope.GetScriptState(), chained_promise);
  get_type_tester.WaitUntilSettled();
  EXPECT_TRUE(get_type_tester.IsFulfilled());

  // No histogram should be recorded yet (SingleSampleMetric not yet destroyed).
  histogram_tester.ExpectTotalCount(
      "Blink.Clipboard.LazyRead.FormatsNeverRead", 0);

  // Simulate frame detachment — triggers ContextDestroyed() which resets the
  // SingleSampleMetrics, emitting the final sample.
  GetFrame().DomWindow()->FrameDestroyed();

  // Histogram should now record that 1 format was never read
  // (2 total MIME types - 1 read = 1 never read).
  histogram_tester.ExpectUniqueSample(
      "Blink.Clipboard.LazyRead.FormatsNeverRead", 1, 1);
}

// Tests that Blink.Clipboard.LazyRead.TotalBlobSizeKB histogram is recorded
// when a lazy ClipboardItem's SingleSampleMetric is destroyed after getType().
TEST_F(ClipboardTest, LazyReadTotalBlobSizeKB_Histogram) {
  base::HistogramTester histogram_tester;
  V8TestingScope scope;
  ExecutionContext* executionContext = GetFrame().DomWindow();

  // Write ~3 KB of plain text (3072 chars ≈ 3 KB).
  String plain_text = String(Vector<UChar>(3072, 'A'));
  WritePlainTextToClipboard(plain_text);
  // Write ~2 KB of HTML (2048 chars ≈ 2 KB).
  String html_text = String(Vector<UChar>(2048, 'B'));
  WriteHtmlToClipboard(html_text);

  EXPECT_CALL(permission_service_, RequestPermission)
      .WillOnce(WithArg<1>(
          [](mojom::blink::PermissionService::RequestPermissionCallback
                 callback) {
            std::move(callback).Run(
                mojom::blink::PermissionStatusWithDetails::New(
                    mojom::blink::PermissionStatus::GRANTED, nullptr));
          }));
  BindMockPermissionService(executionContext);
  SetSecureOrigin(executionContext);
  SetPageFocus(true);

  // Read clipboard items — creates lazy ClipboardItem.
  ScriptPromise<IDLSequence<ClipboardItem>> promise =
      ClipboardPromise::CreateForRead(executionContext, scope.GetScriptState(),
                                      nullptr, scope.GetExceptionState());
  ScriptPromiseTester read_tester(scope.GetScriptState(), promise);
  read_tester.WaitUntilSettled();
  ASSERT_TRUE(read_tester.IsFulfilled());

  // Fetch only the text/html type to trigger blob creation for ~2 KB.
  auto* get_type_helper =
      MakeGarbageCollected<ClipboardItemGetType>("text/html");
  auto chained_promise = promise.Then(scope.GetScriptState(), get_type_helper);
  ScriptPromiseTester get_type_tester(scope.GetScriptState(), chained_promise);
  get_type_tester.WaitUntilSettled();
  EXPECT_TRUE(get_type_tester.IsFulfilled());

  // No histogram should be recorded yet (SingleSampleMetric not yet destroyed).
  histogram_tester.ExpectTotalCount("Blink.Clipboard.LazyRead.TotalBlobSizeKB",
                                    0);

  // Simulate frame detachment — triggers ContextDestroyed() which resets the
  // SingleSampleMetrics, emitting the final sample.
  GetFrame().DomWindow()->FrameDestroyed();

  // Histogram should record only ~2 KB because only text/html was
  // fetched — text/plain was never requested in lazy mode.
  histogram_tester.ExpectUniqueSample(
      "Blink.Clipboard.LazyRead.TotalBlobSizeKB", 2, 1);
}

// Tests that calling getType() for all available formats results in
// FormatsNeverRead=0 and TotalBlobSizeKB reflecting cumulative blob size.
TEST_F(ClipboardTest, LazyReadMultipleGetType_Histogram) {
  base::HistogramTester histogram_tester;
  V8TestingScope scope;
  ExecutionContext* executionContext = GetFrame().DomWindow();

  // Write ~3 KB of plain text and ~2 KB of HTML.
  String plain_text = String(Vector<UChar>(3072, 'A'));
  WritePlainTextToClipboard(plain_text);
  String html_text = String(Vector<UChar>(2048, 'B'));
  WriteHtmlToClipboard(html_text);

  EXPECT_CALL(permission_service_, RequestPermission)
      .WillOnce(WithArg<1>(
          [](mojom::blink::PermissionService::RequestPermissionCallback
                 callback) {
            std::move(callback).Run(
                mojom::blink::PermissionStatusWithDetails::New(
                    mojom::blink::PermissionStatus::GRANTED, nullptr));
          }));
  BindMockPermissionService(executionContext);
  SetSecureOrigin(executionContext);
  SetPageFocus(true);

  ScriptPromise<IDLSequence<ClipboardItem>> promise =
      ClipboardPromise::CreateForRead(executionContext, scope.GetScriptState(),
                                      nullptr, scope.GetExceptionState());
  ScriptPromiseTester read_tester(scope.GetScriptState(), promise);
  read_tester.WaitUntilSettled();
  ASSERT_TRUE(read_tester.IsFulfilled());

  // Call getType() for text/plain.
  auto* get_text = MakeGarbageCollected<ClipboardItemGetType>("text/plain");
  auto text_promise = promise.Then(scope.GetScriptState(), get_text);
  ScriptPromiseTester text_tester(scope.GetScriptState(), text_promise);
  text_tester.WaitUntilSettled();
  EXPECT_TRUE(text_tester.IsFulfilled());

  // Call getType() for text/html.
  auto* get_html = MakeGarbageCollected<ClipboardItemGetType>("text/html");
  auto html_promise = promise.Then(scope.GetScriptState(), get_html);
  ScriptPromiseTester html_tester(scope.GetScriptState(), html_promise);
  html_tester.WaitUntilSettled();
  EXPECT_TRUE(html_tester.IsFulfilled());

  histogram_tester.ExpectTotalCount("Blink.Clipboard.LazyRead.TotalBlobSizeKB",
                                    0);
  histogram_tester.ExpectTotalCount(
      "Blink.Clipboard.LazyRead.FormatsNeverRead", 0);

  // Trigger ContextDestroyed() to emit SingleSampleMetrics.
  GetFrame().DomWindow()->FrameDestroyed();

  // All formats were requested, so FormatsNeverRead should be 0.
  histogram_tester.ExpectUniqueSample(
      "Blink.Clipboard.LazyRead.FormatsNeverRead", 0, 1);

  // TotalBlobSizeKB should reflect cumulative size (~3 KB + ~2 KB = ~5 KB).
  histogram_tester.ExpectUniqueSample(
      "Blink.Clipboard.LazyRead.TotalBlobSizeKB", 5, 1);
}

// Tests that Blink.Clipboard.EagerRead.TotalBlobSizeKB histogram is recorded
// when clipboard.read() completes in eager (non-lazy) mode.
TEST_F(ClipboardTest, EagerReadTotalBlobSizeKB_Histogram) {
  ScopedReadClipboardDataOnClipboardItemGetTypeForTest disable_lazy(false);
  base::HistogramTester histogram_tester;
  V8TestingScope scope;
  ExecutionContext* executionContext = GetFrame().DomWindow();

  // Write ~3 KB of plain text (3072 chars ≈ 3 KB).
  String plain_text = String(Vector<UChar>(3072, 'A'));
  WritePlainTextToClipboard(plain_text);
  // Write ~2 KB of HTML (2048 chars ≈ 2 KB).
  String html_text = String(Vector<UChar>(2048, 'B'));
  WriteHtmlToClipboard(html_text);

  EXPECT_CALL(permission_service_, RequestPermission)
      .WillOnce(WithArg<1>(
          [](mojom::blink::PermissionService::RequestPermissionCallback
                 callback) {
            std::move(callback).Run(
                mojom::blink::PermissionStatusWithDetails::New(
                    mojom::blink::PermissionStatus::GRANTED, nullptr));
          }));
  BindMockPermissionService(executionContext);
  SetSecureOrigin(executionContext);
  SetPageFocus(true);

  // Read clipboard in eager mode (lazy feature disabled).
  ScriptPromise<IDLSequence<ClipboardItem>> promise =
      ClipboardPromise::CreateForRead(executionContext, scope.GetScriptState(),
                                      nullptr, scope.GetExceptionState());
  ScriptPromiseTester read_tester(scope.GetScriptState(), promise);
  read_tester.WaitUntilSettled();
  ASSERT_TRUE(read_tester.IsFulfilled());

  // Histogram should record ~5 KB because eager mode reads all
  // MIME types: ~3 KB text/plain + ~2 KB text/html.
  histogram_tester.ExpectUniqueSample(
      "Blink.Clipboard.EagerRead.TotalBlobSizeKB", 5, 1);
}

// Tests that Blink.Clipboard.Reader.ProcessedDataNull histogram is recorded
// the text clipboard reader receives empty data from the OS clipboard.
TEST_F(ClipboardTest, ReaderProcessedDataNull_EmptyText) {
  ScopedReadClipboardDataOnClipboardItemGetTypeForTest disable_lazy(false);
  base::HistogramTester histogram_tester;
  V8TestingScope scope;
  ExecutionContext* executionContext = GetFrame().DomWindow();

  // Force text/plain to be advertised but leave the actual text data empty.
  // This simulates the OS clipboard reporting a format but returning no data.
  mock_clipboard_host()->AddFormatWithoutData("text/plain");

  EXPECT_CALL(permission_service_, RequestPermission)
      .WillOnce(WithArg<1>(
          [](mojom::blink::PermissionService::RequestPermissionCallback
                 callback) {
            std::move(callback).Run(
                mojom::blink::PermissionStatusWithDetails::New(
                    mojom::blink::PermissionStatus::GRANTED, nullptr));
          }));
  BindMockPermissionService(executionContext);
  SetSecureOrigin(executionContext);
  SetPageFocus(true);

  // Read clipboard in eager mode — text reader will get empty data.
  ScriptPromise<IDLSequence<ClipboardItem>> promise =
      ClipboardPromise::CreateForRead(executionContext, scope.GetScriptState(),
                                      nullptr, scope.GetExceptionState());
  ScriptPromiseTester read_tester(scope.GetScriptState(), promise);
  read_tester.WaitUntilSettled();
  ASSERT_TRUE(read_tester.IsFulfilled());

  // Histogram should record true because the text reader received empty data.
  histogram_tester.ExpectUniqueSample(
      "Blink.Clipboard.Reader.ProcessedDataNull", true, 1);
}

// Tests that Blink.Clipboard.LazyRead.GetTypeRejected boolean histogram is
// recorded when getType() is rejected due to clipboard content change.
TEST_F(ClipboardTest, LazyReadGetTypeRejected_Histogram) {
  base::HistogramTester histogram_tester;
  V8TestingScope scope;
  ExecutionContext* executionContext = GetFrame().DomWindow();

  String testing_string = "TestForRejectedCount";
  WritePlainTextToClipboard(testing_string);

  EXPECT_CALL(permission_service_, RequestPermission)
      .WillOnce(WithArg<1>(
          [](mojom::blink::PermissionService::RequestPermissionCallback
                 callback) {
            std::move(callback).Run(
                mojom::blink::PermissionStatusWithDetails::New(
                    mojom::blink::PermissionStatus::GRANTED, nullptr));
          }));
  BindMockPermissionService(executionContext);
  SetSecureOrigin(executionContext);
  SetPageFocus(true);

  // Read clipboard items (creates lazy ClipboardItem with current seq number).
  ScriptPromise<IDLSequence<ClipboardItem>> promise =
      ClipboardPromise::CreateForRead(executionContext, scope.GetScriptState(),
                                      nullptr, scope.GetExceptionState());
  ScriptPromiseTester read_tester(scope.GetScriptState(), promise);
  read_tester.WaitUntilSettled();
  ASSERT_TRUE(read_tester.IsFulfilled());

  // Change clipboard so getType() will be rejected.
  mock_clipboard_host()->Reset();
  WritePlainTextToClipboard("ChangedContent");
  GetFrame().GetSystemClipboard()->CommitWrite();
  // Flush pending mojo messages so the mock processes CommitWrite and updates
  // its sequence number before getType() checks it.
  test::RunPendingTasks();

  // Extract the ClipboardItem from the resolved read promise.
  v8::Local<v8::Value> read_value = read_tester.Value().V8Value();
  HeapVector<Member<ClipboardItem>> clipboard_items =
      NativeValueTraits<IDLSequence<ClipboardItem>>::NativeValue(
          scope.GetIsolate(), read_value, scope.GetExceptionState());
  ASSERT_FALSE(clipboard_items.empty());

  // Call getType() directly — should throw due to clipboard change.
  DummyExceptionStateForTesting get_type_exception;
  clipboard_items[0]->getType(scope.GetScriptState(), "text/plain",
                              get_type_exception);
  EXPECT_TRUE(get_type_exception.HadException());

  // Histogram should record true (rejected due to clipboard change).
  histogram_tester.ExpectUniqueSample(
      "Blink.Clipboard.LazyRead.GetTypeRejected", true, 1);
}

// Regression test for crbug.com/474131935: readText() must not block the
// renderer thread while waiting for the host's reply.
TEST_F(ClipboardTest, ReadTextIsAsyncWhenClipboardReadIsSlow) {
  V8TestingScope scope;
  ExecutionContext* executionContext = GetFrame().DomWindow();
  String testing_string = "DelayedClipboardText";
  WritePlainTextToClipboard(testing_string);

  EXPECT_CALL(permission_service_, RequestPermission)
      .WillOnce(WithArg<1>(
          [](mojom::blink::PermissionService::RequestPermissionCallback
                 callback) {
            std::move(callback).Run(
                mojom::blink::PermissionStatusWithDetails::New(
                    mojom::blink::PermissionStatus::GRANTED, nullptr));
          }));
  BindMockPermissionService(executionContext);
  SetSecureOrigin(executionContext);
  SetPageFocus(true);

  // Defer the host's ReadText reply to simulate a slow OS clipboard read.
  mock_clipboard_host()->SetReadTextCallbackDeferred(true);

  ScriptPromise<IDLString> promise = ClipboardPromise::CreateForReadText(
      executionContext, scope.GetScriptState(), scope.GetExceptionState());
  ScriptPromiseTester promise_tester(scope.GetScriptState(), promise);

  // Drain the message loop so the IPC reaches the host. If the renderer were
  // blocking on the OS read, control would never return here.
  test::RunPendingTasks();

  // The host received the request, but the promise is still pending.
  EXPECT_EQ(mock_clipboard_host()->ReadTextCallCount(), 1);
  EXPECT_TRUE(mock_clipboard_host()->HasDeferredReadTextCallback());
  EXPECT_FALSE(promise_tester.IsFulfilled());
  EXPECT_FALSE(promise_tester.IsRejected());

  // Let the host respond; the promise must fulfill with the text.
  mock_clipboard_host()->RunDeferredReadTextCallback();
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsFulfilled());
  String returned_string;
  promise_tester.Value().ToString(returned_string);
  EXPECT_EQ(returned_string, testing_string);

  executionContext->GetBrowserInterfaceBroker().SetBinderForTesting(
      mojom::blink::PermissionService::Name_, {});
}

}  // namespace blink
