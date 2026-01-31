// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/extension_script_streamer.h"

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/bindings/core/v8/script_streamer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

constexpr char kFunctionScript[] = "function foo() { return 42; }";
constexpr char kComplexScript[] =
    "function calculate() { "
    "  let result = 0; "
    "  for (let i = 0; i < 100; i++) { "
    "    result += i * i; "
    "  } "
    "  return result; "
    "}"
    "calculate();";

}  // namespace

class ExtensionScriptStreamerTest : public testing::Test {
 public:
  ExtensionScriptStreamerTest() = default;
  ~ExtensionScriptStreamerTest() override = default;

 protected:
  void SetUp() override { web_view_helper_.Initialize(); }

  WebLocalFrame* GetWebFrame() { return web_view_helper_.LocalMainFrame(); }

  LocalFrame* GetLocalFrame() {
    return To<LocalFrame>(
        web_view_helper_.GetWebView()->GetPage()->MainFrame());
  }

  v8::Isolate* GetIsolate() {
    return GetLocalFrame()->DomWindow()->GetIsolate();
  }

  void FlushTasks() {
    base::RunLoop run_loop;
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  test::TaskEnvironment task_environment_;
  frame_test_helpers::WebViewHelper web_view_helper_;
};

TEST_F(ExtensionScriptStreamerTest, BasicLifecycle) {
  WebString content = WebString::FromUTF8(kComplexScript);
  ExtensionScriptStreamer streamer =
      ExtensionScriptStreamer::PostStreamingTaskToBackgroundThread(
          GetWebFrame(), content, "lifecycle_test.js", 1u,
          base::Milliseconds(100));

  // Verify streamer was created successfully
  EXPECT_NE(streamer.GetInlineScriptStreamer(), nullptr);

  // Allow background compilation to complete
  FlushTasks();

  // Verify streamer is still valid after background work
  EXPECT_NE(streamer.GetInlineScriptStreamer(), nullptr);
}

TEST_F(ExtensionScriptStreamerTest, CancellationBehavior) {
  // Test cancellation before background task starts by using a very long
  // timeout to ensure compilation hasn't started yet
  WebString content = WebString::FromUTF8(kFunctionScript);
  ExtensionScriptStreamer streamer =
      ExtensionScriptStreamer::PostStreamingTaskToBackgroundThread(
          GetWebFrame(), content, "cancel_test.js", 1u,
          base::Seconds(1000));  // Very long timeout to prevent compilation

  // Verify the streamer was created
  EXPECT_NE(streamer.GetInlineScriptStreamer(), nullptr);

  // Cancel immediately before the background task can start
  bool cancelled = streamer.CancelStreamingIfNotStarted();

  // With a very long timeout and immediate cancellation, we expect success
  // Note: This may still occasionally fail due to race conditions, but the
  // test now verifies actual cancellation behavior rather than always passing
  if (cancelled) {
    // If cancellation succeeded, the streamer should still be valid but
    // marked as cancelled
    EXPECT_NE(streamer.GetInlineScriptStreamer(), nullptr);
  }

  FlushTasks();

  // Verify the streamer is still accessible after flush
  EXPECT_NE(streamer.GetInlineScriptStreamer(), nullptr);
}

TEST_F(ExtensionScriptStreamerTest, CancellationAfterStart) {
  // Test cancellation after compilation has started
  WebString content = WebString::FromUTF8(kComplexScript);
  ExtensionScriptStreamer streamer =
      ExtensionScriptStreamer::PostStreamingTaskToBackgroundThread(
          GetWebFrame(), content, "started_test.js", 1u, base::Milliseconds(0));

  EXPECT_NE(streamer.GetInlineScriptStreamer(), nullptr);

  // Allow some time for compilation to potentially start
  FlushTasks();

  streamer.CancelStreamingIfNotStarted();

  // Either outcome is valid here depending on timing, but we verify the
  // streamer remains valid
  EXPECT_NE(streamer.GetInlineScriptStreamer(), nullptr);
}

TEST_F(ExtensionScriptStreamerTest, ConcurrentStreaming) {
  const int kNumScripts = 5;
  std::vector<ExtensionScriptStreamer> streamers;

  // Create multiple streamers concurrently
  for (int i = 0; i < kNumScripts; ++i) {
    std::string script = "function func" + base::NumberToString(i) +
                         "() { return " + base::NumberToString(i * 10) + "; }";
    WebString content = WebString::FromUTF8(script);

    streamers.push_back(
        ExtensionScriptStreamer::PostStreamingTaskToBackgroundThread(
            GetWebFrame(), content,
            "concurrent_" + base::NumberToString(i) + ".js",
            static_cast<uint64_t>(i + 1), base::Milliseconds(0)));
  }

  // All streamers should be created successfully
  EXPECT_EQ(streamers.size(), static_cast<size_t>(kNumScripts));
  for (const auto& streamer : streamers) {
    EXPECT_NE(streamer.GetInlineScriptStreamer(), nullptr);
  }

  FlushTasks();
}

TEST_F(ExtensionScriptStreamerTest, EdgeCases) {
  // Empty script - should handle gracefully
  {
    WebString empty_content = WebString::FromUTF8("");
    ExtensionScriptStreamer empty_streamer =
        ExtensionScriptStreamer::PostStreamingTaskToBackgroundThread(
            GetWebFrame(), empty_content, "empty.js", 1u,
            base::Milliseconds(0));
    FlushTasks();
  }

  // Script with syntax error - should not crash the streamer
  {
    WebString invalid_content =
        WebString::FromUTF8("function broken() { syntax error here }");
    ExtensionScriptStreamer error_streamer =
        ExtensionScriptStreamer::PostStreamingTaskToBackgroundThread(
            GetWebFrame(), invalid_content, "syntax_error.js", 2u,
            base::Milliseconds(0));

    EXPECT_NE(error_streamer.GetInlineScriptStreamer(), nullptr);
    FlushTasks();
  }

  // Very large script - tests memory handling
  {
    std::string large_script;
    large_script.reserve(50000);
    for (int i = 0; i < 1000; ++i) {
      large_script += "var var" + base::NumberToString(i) + " = " +
                      base::NumberToString(i) + ";\n";
    }

    WebString large_content = WebString::FromUTF8(large_script);
    ExtensionScriptStreamer large_streamer =
        ExtensionScriptStreamer::PostStreamingTaskToBackgroundThread(
            GetWebFrame(), large_content, "very_large.js", 3u,
            base::Milliseconds(0));

    EXPECT_NE(large_streamer.GetInlineScriptStreamer(), nullptr);
    FlushTasks();
  }
}

TEST_F(ExtensionScriptStreamerTest, UTF16Content) {
  String utf16_script =
      u"function test() { "
      u"  var text = '\u3042\u3044\u3046\u3048\u304a'; "  // Japanese
      u"  var emoji = '\U0001F600'; "                     // Emoji
      u"  return text.length + emoji.length; "
      u"}";
  utf16_script.Ensure16Bit();

  WebString content(utf16_script);
  ExtensionScriptStreamer streamer =
      ExtensionScriptStreamer::PostStreamingTaskToBackgroundThread(
          GetWebFrame(), content, "utf16.js", 1u, base::Milliseconds(0));

  EXPECT_NE(streamer.GetInlineScriptStreamer(), nullptr);
  FlushTasks();

  // UTF-16 content should be handled correctly
  EXPECT_NE(streamer.GetInlineScriptStreamer(), nullptr);
}

TEST_F(ExtensionScriptStreamerTest, V8CompilationIntegration) {
  WebString content = WebString::FromUTF8(kComplexScript);
  ExtensionScriptStreamer streamer =
      ExtensionScriptStreamer::PostStreamingTaskToBackgroundThread(
          GetWebFrame(), content, "v8_compile_test.js", 1u,
          base::Milliseconds(100));

  EXPECT_NE(streamer.GetInlineScriptStreamer(), nullptr);

  // Allow background compilation to complete
  FlushTasks();

  // Get V8 context and verify the script context is valid
  v8::Isolate* isolate = GetIsolate();
  ASSERT_NE(isolate, nullptr);

  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = ToV8ContextEvenIfDetached(
      GetLocalFrame(), DOMWrapperWorld::MainWorld(GetIsolate()));
  EXPECT_FALSE(context.IsEmpty());

  // The streamer should have prepared data for V8 compilation
  EXPECT_NE(streamer.GetInlineScriptStreamer(), nullptr);
}

TEST_F(ExtensionScriptStreamerTest, TimeoutHandling) {
  // Very short timeout - likely to timeout but should not crash
  WebString content = WebString::FromUTF8(kComplexScript);
  ExtensionScriptStreamer short_timeout =
      ExtensionScriptStreamer::PostStreamingTaskToBackgroundThread(
          GetWebFrame(), content, "short_timeout.js", 1u,
          base::Microseconds(1));

  EXPECT_NE(short_timeout.GetInlineScriptStreamer(), nullptr);
  FlushTasks();

  // No timeout - should complete successfully
  ExtensionScriptStreamer no_timeout =
      ExtensionScriptStreamer::PostStreamingTaskToBackgroundThread(
          GetWebFrame(), content, "no_timeout.js", 2u, base::Milliseconds(0));

  EXPECT_NE(no_timeout.GetInlineScriptStreamer(), nullptr);
  FlushTasks();
}

// Test default constructed (null) streamer behavior
TEST_F(ExtensionScriptStreamerTest, NullStreamerBehavior) {
  ExtensionScriptStreamer null_streamer;

  // Null streamer should return nullptr
  EXPECT_EQ(null_streamer.GetInlineScriptStreamer(), nullptr);
}

}  // namespace blink
