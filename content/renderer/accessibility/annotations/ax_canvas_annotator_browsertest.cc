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
  virtual ui::AXMode InitialAXMode() const {
    ui::AXMode mode = ui::kAXModeComplete;
    mode.set_mode(ui::AXMode::kScreenReader, true);
    return mode;
  }

  void SetUp() override {
    RenderAccessibilityImplTest::SetUp();
    base::DiscardableMemoryAllocator::SetInstance(
        &discardable_memory_allocator_);
    GetMainRenderFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
        screen_ai::mojom::ScreenAIAnnotator::Name_,
        base::BindRepeating(&FakeScreenAIAnnotator::BindReceiver,
                            base::Unretained(&fake_annotator_)));
    SetMode(InitialAXMode());
  }

  void TearDown() override {
    GetMainRenderFrame()->GetBrowserInterfaceBroker().SetBinderForTesting(
        screen_ai::mojom::ScreenAIAnnotator::Name_, {});
    fake_annotator_.DisconnectAll();
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

  void LoadCanvasPage(
      const char* body_html =
          R"(<canvas id="c" width="100" height="100"></canvas>)") {
    std::string html = base::StringPrintf("<body>%s</body>", body_html);
    LoadHTMLAndRefreshAccessibilityTree(html.c_str());
  }

  // Draws `color`, advances past Blink's OCR debounce delay, waits for the
  // resulting Mojo OCR request, and returns its 0-based callback index.
  size_t DrawAndWaitForOcrRequest(SkColor color) {
    const size_t request_index = fake_annotator_.call_count();
    DrawSolidBitmap(color);
    AdvancePastOcrDelayAndSerialize();
    fake_annotator_.WaitForCalls(request_index + 1);
    return request_index;
  }

  // Runs a complete draw -> OCR request -> Mojo response -> AX flush cycle and
  // returns the latest serialized kCanvasAnnotation value.
  std::string AnnotateCanvasWithOcr(SkColor color,
                                    const std::string& ocr_text) {
    const size_t request_index = DrawAndWaitForOcrRequest(color);
    fake_annotator_.RunCallback(request_index, {ocr_text});
    SendPendingAccessibilityEvents();
    return GetLatestCanvasAnnotation();
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
  LoadCanvasPage();

  const size_t first_req = DrawAndWaitForOcrRequest(SK_ColorRED);
  EXPECT_EQ(fake_annotator_.call_count(), 1u);
  EXPECT_EQ(fake_annotator_.client_type(),
            screen_ai::mojom::OcrClientType::kCanvas);

  fake_annotator_.RunCallback(first_req, {"First frame"});
  SendPendingAccessibilityEvents();
  EXPECT_EQ(GetLatestCanvasAnnotation(), "First frame");

  // Redrawing identical pixels must preserve the cached annotation without
  // dispatching a duplicate PerformOcrAndReturnAnnotation request.
  DrawSolidBitmap(SK_ColorRED);
  AdvancePastOcrDelayAndSerialize();
  EXPECT_EQ(fake_annotator_.call_count(), 1u);
  EXPECT_EQ(GetLatestCanvasAnnotation(), "First frame");

  // Drawing distinct pixels must dispatch a new OCR request.
  EXPECT_EQ(AnnotateCanvasWithOcr(SK_ColorGREEN, "Second frame"),
            "Second frame");
  EXPECT_EQ(fake_annotator_.call_count(), 2u);
}

TEST_F(AXCanvasAnnotatorTest, WaitsForPendingOcrBeforeDispatchingNextFrame) {
  LoadCanvasPage();

  const size_t first_req = DrawAndWaitForOcrRequest(SK_ColorRED);
  EXPECT_EQ(fake_annotator_.call_count(), 1u);

  // Mutating the canvas while the first OCR request is still in flight must
  // not dispatch a concurrent second request yet, while keeping
  // HasRequestedOCR() armed for the follow-up serialization pass.
  DrawSolidBitmap(SK_ColorGREEN);
  AdvancePastOcrDelayAndSerialize();
  EXPECT_EQ(fake_annotator_.call_count(), 1u);

  // Completing the first request triggers immediate re-serialization, which
  // now dispatches OCR for the updated frame.
  fake_annotator_.RunCallback(first_req, {"First"});
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
  LoadCanvasPage(R"(<canvas id="c" width="100" height="100" )"
                 R"(style="width: 100px; height: 100px;"></canvas>)");

  EXPECT_EQ(AnnotateCanvasWithOcr(SK_ColorRED, "Initial"), "Initial");

  // Start a second OCR request and keep it in flight.
  const size_t pending_req = DrawAndWaitForOcrRequest(SK_ColorGREEN);
  EXPECT_EQ(fake_annotator_.call_count(), 2u);

  // Queue another OCR request and collapse the backing buffer to 0 width
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
  // the now-empty canvas in RefreshCanvasAnnotation() and evicts the cached
  // annotation instead of leaving stale callback text exposed.
  fake_annotator_.RunCallback(pending_req, {"Stale"});
  SendPendingAccessibilityEvents();
  EXPECT_EQ(fake_annotator_.call_count(), 2u);
  EXPECT_TRUE(GetLatestCanvasAnnotation().empty());

  // Restoring the canvas dimensions and drawing red pixels again must re-run
  // OCR because clearing the canvas also reset last_bitmap_hash.
  ExecuteJavaScriptForTests(
      "setTimeout(() => { document.getElementById('c').width = 100; }, 0);");
  task_environment_.FastForwardBy(base::Milliseconds(10));
  EXPECT_EQ(AnnotateCanvasWithOcr(SK_ColorRED, "Restored"), "Restored");
}

TEST_F(AXCanvasAnnotatorTest, CancelsPendingOcrOnNavigation) {
  LoadCanvasPage();
  const size_t pending_req = DrawAndWaitForOcrRequest(SK_ColorRED);

  // Navigating creates a new main document, calling DidCreateNewDocument() on
  // AXCanvasAnnotator so `annotator_remote_` and `canvas_annotations_` are
  // reset.
  LoadCanvasPage();
  ClearHandledUpdates();

  fake_annotator_.RunCallback(pending_req, {"Late navigation text"});
  SendPendingAccessibilityEvents();
  EXPECT_TRUE(GetHandledAccUpdates().empty());

  EXPECT_EQ(AnnotateCanvasWithOcr(SK_ColorGREEN, "New page"), "New page");
}

TEST_F(AXCanvasAnnotatorTest,
       DetachedCanvasCallbackClearsPendingStateAndAllowsRetry) {
  LoadCanvasPage();
  const size_t stale_req = DrawAndWaitForOcrRequest(SK_ColorRED);

  // Remove the canvas from the DOM while OCR is in flight so its AXObject is
  // detached without disconnecting `annotator_remote_`. Deliver the Mojo
  // callback before flushing layout/AX so OnCanvasAnnotated() exercises
  // UpdateAXForAllDocuments() and live detach cleanup.
  ExecuteJavaScriptForTests(
      "window.savedCanvas = document.getElementById('c');"
      "window.savedCanvas.remove();");

  // Delivering the OCR callback while detached must erase the entry by `ax_id`
  // rather than leaving `is_pending` wedged due to WebAXObject::AxID()
  // returning -1.
  fake_annotator_.RunCallback(stale_req, {"Stale detached text"});
  SendPendingAccessibilityEvents();

  // Re-inserting the same canvas element recreates its AXObject with the same
  // DOMNodeId; OCR must run cleanly without being blocked by stale pending or
  // hash state.
  ExecuteJavaScriptForTests("document.body.appendChild(window.savedCanvas);");
  SendPendingAccessibilityEvents();
  EXPECT_EQ(AnnotateCanvasWithOcr(SK_ColorRED, "Restored after reattach"),
            "Restored after reattach");
}

TEST_F(AXCanvasAnnotatorTest, SerializedDetachWhilePendingDropsLateCallback) {
  LoadCanvasPage();
  const size_t stale_req = DrawAndWaitForOcrRequest(SK_ColorGREEN);

  // Serializing a DOM removal prunes the detached pending entry on the update,
  // so re-inserting the canvas before the pre-detach reply arrives ignores the
  // stale callback and allows a fresh OCR pass.
  ExecuteJavaScriptForTests(
      "window.savedCanvas = document.getElementById('c');"
      "window.savedCanvas.remove();");
  SendPendingAccessibilityEvents();

  ClearHandledUpdates();
  ExecuteJavaScriptForTests("document.body.appendChild(window.savedCanvas);");
  SendPendingAccessibilityEvents();

  fake_annotator_.RunCallback(stale_req, {"Stale pre-detach"});
  SendPendingAccessibilityEvents();
  EXPECT_TRUE(GetLatestCanvasAnnotation().empty());

  EXPECT_EQ(AnnotateCanvasWithOcr(SK_ColorBLUE, "Fresh post-reattach"),
            "Fresh post-reattach");
}

TEST_F(AXCanvasAnnotatorTest,
       PreservesCachedAnnotationAcrossDisplayNoneAndVisible) {
  LoadCanvasPage();
  EXPECT_EQ(AnnotateCanvasWithOcr(SK_ColorRED, "Cached chart"), "Cached chart");
  EXPECT_EQ(fake_annotator_.call_count(), 1u);

  // Hiding the canvas via `display: none` makes its node-backed AXObject
  // ignored without notifying Blink's HTMLCanvasAccessibilityManager; cached
  // OCR text must be preserved so static canvases keep their annotation when
  // shown again.
  ExecuteJavaScriptForTests(
      "document.getElementById('c').style.display = 'none';");
  SendPendingAccessibilityEvents();

  ClearHandledUpdates();
  ExecuteJavaScriptForTests(
      "document.getElementById('c').style.display = 'block';");
  SendPendingAccessibilityEvents();

  EXPECT_EQ(GetLatestCanvasAnnotation(), "Cached chart");
  EXPECT_EQ(fake_annotator_.call_count(), 1u);
}

TEST_F(AXCanvasAnnotatorTest, LateCallbackAfterAXObjectRecreationIsApplied) {
  LoadCanvasPage();
  const size_t pending_req = DrawAndWaitForOcrRequest(SK_ColorGREEN);

  // Changing `display` forces a layout reattach, detaching the previous
  // AXObject and recreating a new AXObject with the same DOMNodeId-backed
  // `ax_id`.
  ExecuteJavaScriptForTests(
      "document.getElementById('c').style.display = 'block';");
  SendPendingAccessibilityEvents();

  // The reply must update the recreated live AXObject rather than being
  // dropped.
  ClearHandledUpdates();
  fake_annotator_.RunCallback(pending_req, {"Fresh"});
  SendPendingAccessibilityEvents();
  EXPECT_EQ(GetLatestCanvasAnnotation(), "Fresh");
}

TEST_F(AXCanvasAnnotatorTest, DisconnectPreservesCachedAnnotationAndRetries) {
  LoadCanvasPage();
  EXPECT_EQ(AnnotateCanvasWithOcr(SK_ColorRED, "Cached"), "Cached");
  EXPECT_EQ(fake_annotator_.bind_count(), 1u);

  // Dispatch a second request, then disconnect the Mojo pipe while it is in
  // flight.
  DrawAndWaitForOcrRequest(SK_ColorGREEN);
  fake_annotator_.DisconnectAll();
  SendPendingAccessibilityEvents();
  EXPECT_EQ(GetLatestCanvasAnnotation(), "Cached");

  // Re-triggering OCR on the exact same green pixels must reconnect and
  // re-dispatch because OnOcrDisconnected() reset `is_pending` and
  // `last_bitmap_hash`.
  EXPECT_EQ(AnnotateCanvasWithOcr(SK_ColorGREEN, "Reconnected"), "Reconnected");
  EXPECT_EQ(fake_annotator_.bind_count(), 2u);
}

TEST_F(AXCanvasAnnotatorTest, CallbackWithDirtyLayoutUpdatesWithoutCrashing) {
  LoadCanvasPage(R"(<div id="spacer" style="height: 20px;"></div>)"
                 R"(<canvas id="c" width="100" height="100"></canvas>)");

  const size_t pending_req = DrawAndWaitForOcrRequest(SK_ColorRED);

  // Dirty style and layout without flushing accessibility events before the
  // asynchronous Mojo OCR reply arrives; OnCanvasAnnotated() must bring the
  // document to kLayoutClean before calling MarkWebAXObjectDirty(), and skip
  // empty OCR line entries when joining lines.
  ExecuteJavaScriptForTests(
      "document.getElementById('spacer').style.height = '80px';"
      "document.body.style.paddingTop = '12px';");
  fake_annotator_.RunCallback(pending_req,
                              {"", "First line", "", "Second line", ""});
  SendPendingAccessibilityEvents();

  EXPECT_EQ(GetLatestCanvasAnnotation(), "First line\nSecond line");
}

class AXCanvasAnnotatorLazyModeTest : public AXCanvasAnnotatorTest {
 protected:
  ui::AXMode InitialAXMode() const override { return ui::kAXModeComplete; }
};

TEST_F(AXCanvasAnnotatorLazyModeTest,
       LazilyCreatesAnnotatorOnScreenReaderModeAndReusesCache) {
  LoadCanvasPage();
  DrawSolidBitmap(SK_ColorRED);
  AdvancePastOcrDelayAndSerialize();

  // Without kScreenReader, AXCanvasAnnotator is not created and no OCR pipe is
  // bound.
  EXPECT_EQ(fake_annotator_.bind_count(), 0u);
  EXPECT_EQ(fake_annotator_.call_count(), 0u);
  EXPECT_TRUE(GetLatestCanvasAnnotation().empty());

  // Enabling kScreenReader lazily creates AXCanvasAnnotator in `ax_annotators_`
  // and processes the pending HasRequestedOCR() state without navigating.
  ui::AXMode screen_reader_mode = ui::kAXModeComplete;
  screen_reader_mode.set_mode(ui::AXMode::kScreenReader, true);
  SetMode(screen_reader_mode);
  SendPendingAccessibilityEvents();
  fake_annotator_.WaitForCalls(1);
  fake_annotator_.RunCallback(0, {"Lazy OCR"});
  SendPendingAccessibilityEvents();
  EXPECT_EQ(GetLatestCanvasAnnotation(), "Lazy OCR");

  // Turning kScreenReader off and back on within the same document preserves
  // the completed cached annotation without re-running OCR.
  SetMode(ui::kAXModeComplete);
  SendPendingAccessibilityEvents();
  ClearHandledUpdates();
  SetMode(screen_reader_mode);
  SendPendingAccessibilityEvents();
  EXPECT_EQ(GetLatestCanvasAnnotation(), "Lazy OCR");
  EXPECT_EQ(fake_annotator_.call_count(), 1u);
}

class AXCanvasAnnotatorOcrDisabledTest : public AXCanvasAnnotatorTest {
 public:
  AXCanvasAnnotatorOcrDisabledTest() {
    override_feature_list_.InitAndDisableFeature(
        ax::mojom::features::kScreenAIOCREnabled);
  }

 private:
  base::test::ScopedFeatureList override_feature_list_;
};

TEST_F(AXCanvasAnnotatorOcrDisabledTest,
       SkipsOcrWhenScreenAiOcrFeatureIsDisabled) {
  LoadCanvasPage();
  DrawSolidBitmap(SK_ColorRED);
  AdvancePastOcrDelayAndSerialize();

  EXPECT_EQ(fake_annotator_.bind_count(), 0u);
  EXPECT_EQ(fake_annotator_.call_count(), 0u);
  EXPECT_TRUE(GetLatestCanvasAnnotation().empty());
}

}  // namespace content
