// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/annotations/ax_canvas_annotator.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/adapters.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/accessibility/render_accessibility_impl_test.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_features.mojom-features.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_update.h"

namespace content {

namespace {

class FakeScreenAIAnnotator : public screen_ai::mojom::ScreenAIAnnotator {
 public:
  FakeScreenAIAnnotator() = default;
  FakeScreenAIAnnotator(const FakeScreenAIAnnotator&) = delete;
  FakeScreenAIAnnotator& operator=(const FakeScreenAIAnnotator&) = delete;
  ~FakeScreenAIAnnotator() override = default;

  void BindReceiver(mojo::ScopedMessagePipeHandle handle) {
    ++bind_count_;
    receivers_.Add(this,
                   mojo::PendingReceiver<screen_ai::mojom::ScreenAIAnnotator>(
                       std::move(handle)));
  }

#if BUILDFLAG(IS_CHROMEOS)
  void PerformOcrAndReturnAXTreeUpdate(
      const SkBitmap& image,
      PerformOcrAndReturnAXTreeUpdateCallback callback) override {
    std::move(callback).Run(ui::AXTreeUpdate());
  }
#endif

  void PerformOcrAndReturnAnnotation(
      const SkBitmap& image,
      PerformOcrAndReturnAnnotationCallback callback) override {
    callbacks_.push_back(std::move(callback));
    if (quit_on_expected_calls_ && callbacks_.size() >= expected_call_count_) {
      std::move(quit_on_expected_calls_).Run();
    }
  }

  void SetClientType(screen_ai::mojom::OcrClientType client_type) override {
    client_type_ = client_type;
  }

  void GetMaxImageDimension(GetMaxImageDimensionCallback callback) override {
    std::move(callback).Run(2048);
  }

  void SetOCRLightMode(bool enabled) override {}

  void IsOCRBusy(IsOCRBusyCallback callback) override {
    std::move(callback).Run(false);
  }

  // Waits until `callbacks_` contains at least `expected_count` requests,
  // quitting via RunLoop::QuitClosure so RenderViewTest's MOCK_TIME does not
  // advance unexpectedly.
  void WaitForCalls(size_t expected_count) {
    if (callbacks_.size() >= expected_count) {
      return;
    }
    expected_call_count_ = expected_count;
    base::RunLoop run_loop;
    quit_on_expected_calls_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void RunCallback(size_t index, const std::vector<std::string>& lines) {
    ASSERT_LT(index, callbacks_.size());
    auto annotation = screen_ai::mojom::VisualAnnotation::New();
    for (const std::string& line_text : lines) {
      auto line_box = screen_ai::mojom::LineBox::New();
      line_box->text_line = line_text;
      annotation->lines.push_back(std::move(line_box));
    }
    std::move(callbacks_[index]).Run(std::move(annotation));
    receivers_.FlushForTesting();
  }

  void DisconnectAll() { receivers_.Clear(); }

  size_t call_count() const { return callbacks_.size(); }
  size_t bind_count() const { return bind_count_; }
  std::optional<screen_ai::mojom::OcrClientType> client_type() const {
    return client_type_;
  }

 private:
  size_t bind_count_ = 0;
  size_t expected_call_count_ = 0;
  std::optional<screen_ai::mojom::OcrClientType> client_type_;
  base::OnceClosure quit_on_expected_calls_;
  std::vector<PerformOcrAndReturnAnnotationCallback> callbacks_;
  mojo::ReceiverSet<screen_ai::mojom::ScreenAIAnnotator> receivers_;
};

const ui::AXNodeData* FindCanvasNode(const ui::AXTreeUpdate& update) {
  for (const auto& node : update.nodes) {
    if (node.role == ax::mojom::Role::kCanvas) {
      return &node;
    }
  }
  return nullptr;
}

}  // namespace

class AXCanvasAnnotatorTest : public RenderAccessibilityImplTest {
 public:
  AXCanvasAnnotatorTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{ax::mojom::features::kScreenAIOCREnabled, {}},
         {features::kAccessibilityCanvas,
          {{"CanvasAccessibilityMode", "Advanced"}}}},
        {});
  }
  AXCanvasAnnotatorTest(const AXCanvasAnnotatorTest&) = delete;
  AXCanvasAnnotatorTest& operator=(const AXCanvasAnnotatorTest&) = delete;
  ~AXCanvasAnnotatorTest() override = default;

 protected:
  void SetUp() override {
    RenderAccessibilityImplTest::SetUp();
    base::DiscardableMemoryAllocator::SetInstance(
        &discardable_memory_allocator_);
    GetMainRenderFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
        screen_ai::mojom::ScreenAIAnnotator::Name_,
        base::BindRepeating(&FakeScreenAIAnnotator::BindReceiver,
                            base::Unretained(&fake_annotator_)));
    ui::AXMode mode = ui::kAXModeComplete;
    mode.set_mode(ui::AXMode::kScreenReader, true);
    SetMode(mode);
  }

  void TearDown() override {
    GetMainRenderFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
        screen_ai::mojom::ScreenAIAnnotator::Name_, {});
    base::DiscardableMemoryAllocator::SetInstance(nullptr);
    RenderAccessibilityImplTest::TearDown();
  }

  // Transfers a solid-color ImageBitmap into the canvas inside a Blink timer
  // task so DidProcessTask runs PostFinalizeFrame() and starts Blink's
  // 1-second OCR debounce timer without requiring a GPU channel in
  // RenderViewTest.
  void DrawSolidBitmap(SkColor color) {
    std::string script = base::StringPrintf(
        "setTimeout(() => {"
        "  const c = document.getElementById('c');"
        "  const ctx = c.getContext('bitmaprenderer');"
        "  const data = new Uint8ClampedArray(16 * 16 * 4);"
        "  for (let i = 0; i < data.length; i += 4) {"
        "    data[i] = %u;"
        "    data[i + 1] = %u;"
        "    data[i + 2] = %u;"
        "    data[i + 3] = %u;"
        "  }"
        "  createImageBitmap(new ImageData(data, 16, 16))"
        "      .then(bmp => ctx.transferFromImageBitmap(bmp));"
        "}, 0);",
        SkColorGetR(color), SkColorGetG(color), SkColorGetB(color),
        SkColorGetA(color));
    ExecuteJavaScriptForTests(script);
  }

  // Advances mock time past Blink's 1-second kOCRDelay and flushes pending
  // accessibility updates to AXCanvasAnnotator.
  void AdvancePastOcrDelayAndSerialize() {
    task_environment_.FastForwardBy(base::Milliseconds(1200));
    SendPendingAccessibilityEvents();
  }

  std::string GetLatestCanvasAnnotation() {
    const auto& updates = GetHandledAccUpdates();
    for (const auto& update : base::Reversed(updates)) {
      if (const ui::AXNodeData* canvas = FindCanvasNode(update)) {
        return canvas->GetStringAttribute(
            ax::mojom::StringAttribute::kCanvasAnnotation);
      }
    }
    return std::string();
  }

  FakeScreenAIAnnotator fake_annotator_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::TestDiscardableMemoryAllocator discardable_memory_allocator_;
};

TEST_F(AXCanvasAnnotatorTest, SkipsDuplicateOcrWhenPixelsAreUnchanged) {
  LoadHTMLAndRefreshAccessibilityTree(R"HTML(
    <body>
      <canvas id="c" width="100" height="100"></canvas>
    </body>
  )HTML");

  DrawSolidBitmap(SK_ColorRED);
  AdvancePastOcrDelayAndSerialize();

  fake_annotator_.WaitForCalls(1);
  EXPECT_EQ(fake_annotator_.call_count(), 1u);
  EXPECT_EQ(fake_annotator_.client_type(),
            screen_ai::mojom::OcrClientType::kCanvas);

  fake_annotator_.RunCallback(0, {"First frame"});
  SendPendingAccessibilityEvents();
  EXPECT_EQ(GetLatestCanvasAnnotation(), "First frame");

  // Redrawing identical pixels must preserve the cached annotation without
  // dispatching a duplicate PerformOcrAndReturnAnnotation request.
  DrawSolidBitmap(SK_ColorRED);
  AdvancePastOcrDelayAndSerialize();
  EXPECT_EQ(fake_annotator_.call_count(), 1u);
  EXPECT_EQ(GetLatestCanvasAnnotation(), "First frame");

  // Drawing distinct pixels must dispatch a new OCR request.
  DrawSolidBitmap(SK_ColorGREEN);
  AdvancePastOcrDelayAndSerialize();
  fake_annotator_.WaitForCalls(2);
  EXPECT_EQ(fake_annotator_.call_count(), 2u);

  fake_annotator_.RunCallback(1, {"Second frame"});
  SendPendingAccessibilityEvents();
  EXPECT_EQ(GetLatestCanvasAnnotation(), "Second frame");
}

TEST_F(AXCanvasAnnotatorTest, WaitsForPendingOcrBeforeDispatchingNextFrame) {
  LoadHTMLAndRefreshAccessibilityTree(R"HTML(
    <body>
      <canvas id="c" width="100" height="100"></canvas>
    </body>
  )HTML");

  DrawSolidBitmap(SK_ColorRED);
  AdvancePastOcrDelayAndSerialize();
  fake_annotator_.WaitForCalls(1);
  EXPECT_EQ(fake_annotator_.call_count(), 1u);

  // Mutating the canvas while the first OCR request is still in flight must
  // not dispatch a concurrent second request yet, while keeping
  // HasRequestedOCR() armed for the follow-up serialization pass.
  DrawSolidBitmap(SK_ColorGREEN);
  AdvancePastOcrDelayAndSerialize();
  EXPECT_EQ(fake_annotator_.call_count(), 1u);

  // Completing the first request triggers immediate re-serialization, which
  // now dispatches OCR for the updated frame.
  fake_annotator_.RunCallback(0, {"First"});
  SendPendingAccessibilityEvents();
  fake_annotator_.WaitForCalls(2);
  EXPECT_EQ(fake_annotator_.call_count(), 2u);

  fake_annotator_.RunCallback(1, {"Second"});
  SendPendingAccessibilityEvents();
  EXPECT_EQ(GetLatestCanvasAnnotation(), "Second");
}

TEST_F(AXCanvasAnnotatorTest,
       EmptyCanvasWhilePendingIsNotOverriddenByLateCallback) {
  // Set explicit CSS size so collapsing `c.width = 0` below keeps the canvas
  // >= 10x10 CSS px and unignored in the AX tree.
  LoadHTMLAndRefreshAccessibilityTree(R"HTML(
    <body>
      <canvas id="c" width="100" height="100"
              style="width: 100px; height: 100px;"></canvas>
    </body>
  )HTML");

  // Establish an initial cached annotation.
  DrawSolidBitmap(SK_ColorRED);
  AdvancePastOcrDelayAndSerialize();
  fake_annotator_.WaitForCalls(1);
  fake_annotator_.RunCallback(0, {"Initial"});
  SendPendingAccessibilityEvents();
  EXPECT_EQ(GetLatestCanvasAnnotation(), "Initial");

  // Start a second OCR request and keep it in flight.
  DrawSolidBitmap(SK_ColorGREEN);
  AdvancePastOcrDelayAndSerialize();
  fake_annotator_.WaitForCalls(2);
  EXPECT_EQ(fake_annotator_.call_count(), 2u);

  // Trigger another OCR request and collapse the backing buffer to 0 width
  // (keeping CSS layout size 100x100 so the canvas stays in the AX tree) while
  // request #2 is still pending.
  DrawSolidBitmap(SK_ColorBLUE);
  task_environment_.FastForwardBy(base::Milliseconds(1200));
  ExecuteJavaScriptForTests(
      "setTimeout(() => { document.getElementById('c').width = 0; }, 0);");
  task_environment_.FastForwardBy(base::Milliseconds(10));
  SendPendingAccessibilityEvents();
  EXPECT_EQ(fake_annotator_.call_count(), 2u);
  EXPECT_EQ(GetLatestCanvasAnnotation(), "Initial");

  // When the in-flight request completes, its follow-up serialization evaluates
  // the now-empty canvas and clears the annotation instead of leaving stale
  // callback text on the node.
  fake_annotator_.RunCallback(1, {"Stale"});
  SendPendingAccessibilityEvents();
  EXPECT_EQ(fake_annotator_.call_count(), 2u);
  EXPECT_TRUE(GetLatestCanvasAnnotation().empty());

  // Restoring the canvas dimensions and drawing red pixels again must re-run
  // OCR because clearing the canvas also reset last_bitmap_hash.
  ExecuteJavaScriptForTests(
      "setTimeout(() => { document.getElementById('c').width = 100; }, 0);");
  task_environment_.FastForwardBy(base::Milliseconds(10));
  DrawSolidBitmap(SK_ColorRED);
  AdvancePastOcrDelayAndSerialize();
  fake_annotator_.WaitForCalls(3);
  fake_annotator_.RunCallback(2, {"Restored"});
  SendPendingAccessibilityEvents();
  EXPECT_EQ(GetLatestCanvasAnnotation(), "Restored");
}

TEST_F(AXCanvasAnnotatorTest, CancelsPendingOcrOnNavigation) {
  LoadHTMLAndRefreshAccessibilityTree(R"HTML(
    <body>
      <canvas id="c" width="100" height="100"></canvas>
    </body>
  )HTML");

  DrawSolidBitmap(SK_ColorRED);
  AdvancePastOcrDelayAndSerialize();
  fake_annotator_.WaitForCalls(1);

  // Navigating commits a new provisional load, which calls CancelAnnotations()
  // (resetting `annotator_remote_` so the in-flight Mojo reply is dropped) and
  // clears `canvas_annotations_` when the new document is annotated.
  LoadHTMLAndRefreshAccessibilityTree(R"HTML(
    <body>
      <canvas id="c" width="100" height="100"></canvas>
    </body>
  )HTML");
  ClearHandledUpdates();

  fake_annotator_.RunCallback(0, {"Late navigation text"});
  SendPendingAccessibilityEvents();
  EXPECT_TRUE(GetHandledAccUpdates().empty());
}

TEST_F(AXCanvasAnnotatorTest, DisconnectPreservesCachedAnnotationAndRetries) {
  LoadHTMLAndRefreshAccessibilityTree(R"HTML(
    <body>
      <canvas id="c" width="100" height="100"></canvas>
    </body>
  )HTML");

  DrawSolidBitmap(SK_ColorRED);
  AdvancePastOcrDelayAndSerialize();
  fake_annotator_.WaitForCalls(1);
  fake_annotator_.RunCallback(0, {"Cached"});
  SendPendingAccessibilityEvents();
  EXPECT_EQ(GetLatestCanvasAnnotation(), "Cached");
  EXPECT_EQ(fake_annotator_.bind_count(), 1u);

  // Dispatch a second request, then disconnect the Mojo pipe while it is in
  // flight.
  DrawSolidBitmap(SK_ColorGREEN);
  AdvancePastOcrDelayAndSerialize();
  fake_annotator_.WaitForCalls(2);
  fake_annotator_.DisconnectAll();
  SendPendingAccessibilityEvents();

  // The previously cached annotation must remain intact across the disconnect.
  EXPECT_EQ(GetLatestCanvasAnnotation(), "Cached");

  // Re-triggering OCR on the exact same green pixels must reconnect and
  // re-dispatch because OnOcrDisconnected() reset `is_pending` and
  // `last_bitmap_hash`.
  DrawSolidBitmap(SK_ColorGREEN);
  AdvancePastOcrDelayAndSerialize();
  fake_annotator_.WaitForCalls(3);
  EXPECT_EQ(fake_annotator_.bind_count(), 2u);

  fake_annotator_.RunCallback(2, {"Reconnected"});
  SendPendingAccessibilityEvents();
  EXPECT_EQ(GetLatestCanvasAnnotation(), "Reconnected");
}

}  // namespace content
