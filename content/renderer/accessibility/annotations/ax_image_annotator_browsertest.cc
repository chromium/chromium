// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/annotations/ax_image_annotator.h"

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "content/common/frame.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/test/frame_load_waiter.h"
#include "content/renderer/accessibility/annotations/ax_annotators_manager.h"
#include "content/renderer/accessibility/render_accessibility_impl.h"
#include "content/renderer/accessibility/render_accessibility_impl_test.h"
#include "content/renderer/accessibility/render_accessibility_manager.h"
#include "content/renderer/render_frame_impl.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/image_annotation/public/mojom/image_annotation.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/scroll/scroll_enums.mojom-shared.h"
#include "third_party/blink/public/web/web_ax_context.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/gfx/geometry/size.h"

namespace content {

using blink::WebAXObject;
using blink::WebDocument;
using testing::ElementsAre;

namespace {

constexpr char kImage1[] =
    "data:image/png;base64,"
    "iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12P4//8/"
    "w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg==";
constexpr char kImage2[] =
    "data:image/png;base64,"
    "iVBORw0KGgoAAAANSUhEUgAAAAoAAAAKCAIAAAACUFjqAAAACXBIWXMAAAsTAAALEwEAmpwYAA"
    "AAB3RJTUUH4gcVABQvx8CBmAAAAB1pVFh0Q29tbWVudAAAAAAAQ3JlYXRlZCB3aXRoIEdJTVBk"
    "LmUHAAAAFUlEQVQY02P8//8/A27AxIAXjFRpAKXjAxH/0Dm5AAAAAElFTkSuQmCC";

// Runs a one-shot callback when layout updates the frame's scroll offset
// synchronously inside UpdateAXForAllDocuments(), quitting via RunLoop rather
// than base::test::RunUntil so RenderViewTest's MOCK_TIME does not advance.
class ScrollOffsetObserver : public RenderFrameObserver {
 public:
  ScrollOffsetObserver(RenderFrame* render_frame, base::OnceClosure callback)
      : RenderFrameObserver(render_frame), callback_(std::move(callback)) {}
  ~ScrollOffsetObserver() override = default;

  void Wait() {
    if (!did_scroll_) {
      run_loop_.Run();
    }
  }

  // RenderFrameObserver:
  void DidChangeScrollOffset(blink::mojom::ScrollType) override {
    did_scroll_ = true;
    base::OnceClosure quit = run_loop_.QuitClosure();
    if (callback_) {
      std::move(callback_).Run();
    }
    std::move(quit).Run();
  }
  void OnDestruct() override {}

 private:
  base::OnceClosure callback_;
  bool did_scroll_ = false;
  base::RunLoop run_loop_;
};

class TestAXImageAnnotator : public AXImageAnnotator {
 public:
  TestAXImageAnnotator(RenderAccessibilityImpl* const render_accessibility)
      : AXImageAnnotator(render_accessibility) {}
  TestAXImageAnnotator(const TestAXImageAnnotator&) = delete;
  TestAXImageAnnotator& operator=(const TestAXImageAnnotator&) = delete;
  ~TestAXImageAnnotator() override = default;

 private:
  std::string GenerateImageSourceId(
      const blink::WebAXObject& image) const override {
    std::string image_id;
    if (image.IsDetached() || image.IsNull() || image.GetNode().IsNull() ||
        image.GetNode().To<blink::WebElement>().IsNull()) {
      ADD_FAILURE() << "Unable to retrieve the image src.";
      return image_id;
    }

    image_id =
        image.GetNode().To<blink::WebElement>().GetAttribute("SRC").Utf8();
    return image_id;
  }
};

class MockImageAnnotationService : public image_annotation::mojom::Annotator {
 public:
  MockImageAnnotationService() = default;
  MockImageAnnotationService(const MockImageAnnotationService&) = delete;
  MockImageAnnotationService& operator=(const MockImageAnnotationService&) =
      delete;
  ~MockImageAnnotationService() override = default;

  mojo::PendingRemote<image_annotation::mojom::Annotator> GetRemote() {
    mojo::PendingRemote<image_annotation::mojom::Annotator> remote;
    receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  void AnnotateImage(
      const std::string& image_id,
      const std::string& /* description_language_tag */,
      mojo::PendingRemote<image_annotation::mojom::ImageProcessor>
          image_processor,
      AnnotateImageCallback callback) override {
    image_ids_.push_back(image_id);
    image_processors_.push_back(
        mojo::Remote<image_annotation::mojom::ImageProcessor>(
            std::move(image_processor)));
    image_processors_.back().set_disconnect_handler(
        base::BindOnce(&MockImageAnnotationService::ResetImageProcessor,
                       base::Unretained(this), image_processors_.size() - 1));
    callbacks_.push_back(std::move(callback));
    if (quit_on_expected_calls_ && callbacks_.size() >= expected_call_count_) {
      std::move(quit_on_expected_calls_).Run();
    }
  }

  // Waits until `callbacks_` contains at least `expected_count` pending
  // requests, quitting via RunLoop::QuitClosure so RenderViewTest's MOCK_TIME
  // does not advance.
  void WaitForCalls(size_t expected_count) {
    if (callbacks_.size() >= expected_count) {
      return;
    }
    expected_call_count_ = expected_count;
    base::RunLoop run_loop;
    quit_on_expected_calls_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  // Tests should not delete entries in these lists.
  std::vector<std::string> image_ids_;
  std::vector<mojo::Remote<image_annotation::mojom::ImageProcessor>>
      image_processors_;
  std::vector<AnnotateImageCallback> callbacks_;

 private:
  void ResetImageProcessor(const size_t index) {
    image_processors_[index].reset();
  }

  size_t expected_call_count_ = 0;
  base::OnceClosure quit_on_expected_calls_;
  mojo::ReceiverSet<image_annotation::mojom::Annotator> receivers_;
};

}  // namespace

class AXImageAnnotatorTest : public RenderAccessibilityImplTest {
 public:
  AXImageAnnotatorTest() = default;
  AXImageAnnotatorTest(const AXImageAnnotatorTest&) = delete;
  AXImageAnnotatorTest& operator=(const AXImageAnnotatorTest&) = delete;
  ~AXImageAnnotatorTest() override = default;

 protected:
  void InstallTestImageAnnotator(RenderAccessibilityImpl* render_accessibility,
                                 MockImageAnnotationService& service) {
    auto annotator =
        std::make_unique<TestAXImageAnnotator>(render_accessibility);
    annotator->BindAnnotatorForTesting(service.GetRemote());
    render_accessibility->ax_annotators_manager_for_testing()
        ->AddAnnotatorForTesting(std::move(annotator));
  }

  void SetUp() override {
    RenderAccessibilityImplTest::SetUp();
    // TODO(nektar): Add the ability to test the AX action that labels images
    // only once.
    ui::AXMode mode = ui::kAXModeComplete;
    mode.set_mode(ui::AXMode::kLabelImages, true);
    SetMode(mode);
    InstallTestImageAnnotator(GetRenderAccessibilityImpl(),
                              mock_annotator_service());
    AXImageAnnotator::IgnoreProtocolChecksForTesting();
    base::DiscardableMemoryAllocator::SetInstance(
        &discardable_memory_allocator);

    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    base::DiscardableMemoryAllocator::SetInstance(nullptr);
    GetRenderAccessibilityImpl()
        ->ax_annotators_manager_for_testing()
        ->ClearAnnotatorsForTesting();
    task_environment_.RunUntilIdle();
    RenderAccessibilityImplTest::TearDown();
  }

  MockImageAnnotationService& mock_annotator_service() {
    return mock_annotator_service_;
  }

 private:
  MockImageAnnotationService mock_annotator_service_;
  base::TestDiscardableMemoryAllocator discardable_memory_allocator;
};

TEST_F(AXImageAnnotatorTest, OnImageAdded) {
  LoadHTMLAndRefreshAccessibilityTree(base::StringPrintf(R"HTML(
      <body>
        <p>Test document</p>
        <img id="A" src="%s"
            style="width: 200px; height: 150px;">
        <img id="B" src="%s"
            style="visibility: hidden; width: 200px; height: 150px;">
      </body>
      )HTML",
                                                         kImage1, kImage2)
                                          .c_str());

  // Every time we call a method on a Mojo interface, a message is posted to the
  // current task queue. We need to ask the queue to drain itself before we
  // check test expectations.
  task_environment_.RunUntilIdle();

  EXPECT_THAT(mock_annotator_service().image_ids_, ElementsAre(kImage1));
  ASSERT_EQ(1u, mock_annotator_service().image_processors_.size());
  EXPECT_TRUE(mock_annotator_service().image_processors_[0].is_bound());
  EXPECT_EQ(1u, mock_annotator_service().callbacks_.size());

  WebDocument document = GetMainFrame()->GetDocument();
  WebAXObject root_obj = WebAXObject::FromWebDocument(document);
  ASSERT_FALSE(root_obj.IsNull());

  // Show node "B".
  ExecuteJavaScriptForTests(
      "document.getElementById('B').style.visibility = 'visible';");
  SendPendingAccessibilityEvents();
  ClearHandledUpdates();

  // This should update the annotations of all images on the page, including the
  // already visible one.
  MarkSubtreeDirty(root_obj);
  SendPendingAccessibilityEvents();

  EXPECT_THAT(mock_annotator_service().image_ids_,
              ElementsAre(kImage1, kImage2, kImage1, kImage2));
  ASSERT_EQ(4u, mock_annotator_service().image_processors_.size());
  EXPECT_TRUE(mock_annotator_service().image_processors_[0].is_bound());
  EXPECT_TRUE(mock_annotator_service().image_processors_[1].is_bound());
  EXPECT_TRUE(mock_annotator_service().image_processors_[2].is_bound());
  EXPECT_TRUE(mock_annotator_service().image_processors_[3].is_bound());
  EXPECT_EQ(4u, mock_annotator_service().callbacks_.size());
}

TEST_F(AXImageAnnotatorTest, OnImageUpdated) {
  LoadHTMLAndRefreshAccessibilityTree(base::StringPrintf(R"HTML(
      <body>
        <p>Test document</p>
        <img src="%s" style="width: 200px; height: 150px;">
      </body>
      )HTML",
                                                         kImage1)
                                          .c_str());

  // Every time we call a method on a Mojo interface, a message is posted to the
  // current task queue. We need to ask the queue to drain itself before we
  // check test expectations.
  task_environment_.RunUntilIdle();

  EXPECT_THAT(mock_annotator_service().image_ids_, ElementsAre(kImage1));
  ASSERT_EQ(1u, mock_annotator_service().image_processors_.size());
  EXPECT_TRUE(mock_annotator_service().image_processors_[0].is_bound());
  EXPECT_EQ(1u, mock_annotator_service().callbacks_.size());

  ClearHandledUpdates();
  WebDocument document = GetMainFrame()->GetDocument();
  WebAXObject root_obj = WebAXObject::FromWebDocument(document);
  ASSERT_FALSE(root_obj.IsNull());

  // This should update the annotations of all images on the page.
  MarkSubtreeDirty(root_obj);
  SendPendingAccessibilityEvents();

  EXPECT_THAT(mock_annotator_service().image_ids_,
              ElementsAre(kImage1, kImage1));
  ASSERT_EQ(2u, mock_annotator_service().image_processors_.size());
  EXPECT_TRUE(mock_annotator_service().image_processors_[0].is_bound());
  EXPECT_TRUE(mock_annotator_service().image_processors_[1].is_bound());
  EXPECT_EQ(2u, mock_annotator_service().callbacks_.size());

  // Update node "A".
  ExecuteJavaScriptForTests(
      base::StringPrintf("document.querySelector('img').src = '%s';", kImage2));
  SendPendingAccessibilityEvents();

  ClearHandledUpdates();
  // This should update the annotations of all images on the page, including the
  // now updated image src.
  SendPendingAccessibilityEvents();

  EXPECT_THAT(mock_annotator_service().image_ids_,
              ElementsAre(kImage1, kImage1, kImage2));
  ASSERT_EQ(3u, mock_annotator_service().image_processors_.size());
  EXPECT_TRUE(mock_annotator_service().image_processors_[0].is_bound());
  EXPECT_TRUE(mock_annotator_service().image_processors_[1].is_bound());
  EXPECT_TRUE(mock_annotator_service().image_processors_[2].is_bound());
  EXPECT_EQ(3u, mock_annotator_service().callbacks_.size());
}

// Verifies that OnImageAnnotated returns safely without accessing freed member
// state if UpdateAXForAllDocuments() synchronously destroys the annotator
// (tested with both error and annotation-result replies).
TEST_F(AXImageAnnotatorTest, OnImageAnnotatedAnnotatorDestroyedDuringUpdate) {
  Resize(gfx::Size(800, 600), /*is_fullscreen=*/false);
  LoadHTMLAndRefreshAccessibilityTree(base::StringPrintf(R"HTML(
      <body>
        <div id="above" style="height: 200px;"></div>
        <div id="anchor" style="height: 2000px;">Test document</div>
        <img id="A" src="%s" style="width: 200px; height: 150px;">
      </body>
      )HTML",
                                                         kImage1)
                                          .c_str());
  mock_annotator_service().WaitForCalls(1u);
  ASSERT_EQ(1u, mock_annotator_service().callbacks_.size());

  // Case 1: Annotator destroyed during an error-code reply.
  // Scroll onto #anchor, then grow #above without flushing layout so that
  // scroll anchoring inside OnImageAnnotated's UpdateAXForAllDocuments() fires
  // DidChangeScrollOffset and synchronously clears the annotators.
  ExecuteJavaScriptForTests("window.scrollTo(0, 300);");
  SendPendingAccessibilityEvents();

  bool destroyed_on_error = false;
  {
    ScrollOffsetObserver observer(frame(), base::BindLambdaForTesting([&]() {
                                    destroyed_on_error = true;
                                    GetRenderAccessibilityImpl()
                                        ->ax_annotators_manager_for_testing()
                                        ->ClearAnnotatorsForTesting();
                                  }));
    ExecuteJavaScriptForTests(
        "document.getElementById('above').style.height = '500px';");
    EXPECT_FALSE(destroyed_on_error);
    std::move(mock_annotator_service().callbacks_[0])
        .Run(image_annotation::mojom::AnnotateImageResult::NewErrorCode(
            image_annotation::mojom::AnnotateImageError::kFailure));
    observer.Wait();
    EXPECT_TRUE(destroyed_on_error);
  }

  // Case 2: Annotator destroyed during a successful kLabel reply.
  MockImageAnnotationService second_service;
  InstallTestImageAnnotator(GetRenderAccessibilityImpl(), second_service);
  GetRenderAccessibilityImpl()->GetAXContext()->MarkDocumentDirty();
  SendPendingAccessibilityEvents();
  second_service.WaitForCalls(1u);
  ASSERT_EQ(1u, second_service.callbacks_.size());

  bool destroyed_on_success = false;
  {
    ScrollOffsetObserver observer(frame(), base::BindLambdaForTesting([&]() {
                                    destroyed_on_success = true;
                                    GetRenderAccessibilityImpl()
                                        ->ax_annotators_manager_for_testing()
                                        ->ClearAnnotatorsForTesting();
                                  }));
    ExecuteJavaScriptForTests(
        "document.getElementById('above').style.height = '800px';");
    EXPECT_FALSE(destroyed_on_success);

    std::vector<image_annotation::mojom::AnnotationPtr> annotations;
    annotations.push_back(image_annotation::mojom::Annotation::New(
        image_annotation::mojom::AnnotationType::kLabel, /*score=*/1.0,
        "Sample label"));
    std::move(second_service.callbacks_[0])
        .Run(image_annotation::mojom::AnnotateImageResult::NewAnnotations(
            std::move(annotations)));
    observer.Wait();
    EXPECT_TRUE(destroyed_on_success);
  }
}

// Regression test for crbug.com/580869107: OnImageAnnotated in a child <iframe>
// promotes a lifecycle update to the parent LocalFrameRoot(), where synchronous
// re-entrancy detaches the child frame (destroying its RenderAccessibilityImpl,
// AXAnnotatorsManager, and AXImageAnnotator while OnImageAnnotated is on the
// stack).
TEST_F(AXImageAnnotatorTest, OnImageAnnotatedChildFrameDetachedDuringUpdate) {
  Resize(gfx::Size(800, 600), /*is_fullscreen=*/false);
  LoadHTMLAndRefreshAccessibilityTree(base::StringPrintf(R"HTML(
      <body>
        <div id="above" style="height: 200px;"></div>
        <div id="anchor" style="height: 2000px;">Anchor text</div>
        <iframe id="victim"
            srcdoc="<p>Child</p><img id='A' src='%s' width='200' height='150'>">
        </iframe>
      </body>
      )HTML",
                                                         kImage1)
                                          .c_str());

  ASSERT_NE(nullptr, GetMainFrame()->FirstChild());
  blink::WebLocalFrame* child_web_frame =
      GetMainFrame()->FirstChild()->ToWebLocalFrame();
  ASSERT_NE(nullptr, child_web_frame);
  auto* child_render_frame =
      static_cast<RenderFrameImpl*>(RenderFrame::FromWebFrame(child_web_frame));
  ASSERT_NE(nullptr, child_render_frame);
  if (child_web_frame->IsLoading()) {
    FrameLoadWaiter(child_render_frame).Wait();
  }
  ASSERT_FALSE(child_web_frame->GetDocument().GetElementById("A").IsNull());

  // Enable accessibility + image labeling on the child RenderFrame and install
  // a TestAXImageAnnotator bound to its own MockImageAnnotationService.
  ui::AXMode mode = ui::kAXModeComplete;
  mode.set_mode(ui::AXMode::kLabelImages, true);
  child_render_frame->GetRenderAccessibilityManager()->SetMode(
      mode,
      /*reset_token=*/1);
  RenderAccessibilityImpl* child_ra =
      child_render_frame->GetRenderAccessibilityManager()
          ->GetRenderAccessibilityImpl();
  ASSERT_NE(nullptr, child_ra);
  child_ra->ax_annotators_manager_for_testing()->ClearAnnotatorsForTesting();

  MockImageAnnotationService child_service;
  InstallTestImageAnnotator(child_ra, child_service);
  child_ra->GetAXContext()->MarkDocumentDirty();
  child_ra->GetAXContext()->UpdateAXForAllDocuments();
  child_service.WaitForCalls(1u);
  ASSERT_EQ(1u, child_service.callbacks_.size());

  // Scroll the parent frame onto #anchor, then grow #above without flushing
  // layout so that scroll anchoring on the parent LocalFrameRoot() fires
  // DidChangeScrollOffset after layout completes inside child_ra's
  // UpdateAXForAllDocuments() call and detaches the child frame via
  // FrameDetached.
  ExecuteJavaScriptForTests("window.scrollTo(0, 300);");
  SendPendingAccessibilityEvents();

  bool child_detached = false;
  ScrollOffsetObserver parent_observer(
      frame(), base::BindLambdaForTesting([&]() {
        child_detached = true;
        // Clear local pointers before Delete(), which runs FrameDetached() ->
        // render_accessibility_manager_.reset() and deletes child_render_frame.
        RenderFrameImpl* victim = std::exchange(child_render_frame, nullptr);
        child_web_frame = nullptr;
        child_ra = nullptr;
        static_cast<mojom::Frame*>(victim)->Delete(
            mojom::FrameDeleteIntention::kNotMainFrame);
      }));

  ExecuteJavaScriptForTests(
      "document.getElementById('above').style.height = '500px';");
  EXPECT_FALSE(child_detached);

  std::move(child_service.callbacks_[0])
      .Run(image_annotation::mojom::AnnotateImageResult::NewErrorCode(
          image_annotation::mojom::AnnotateImageError::kFailure));
  parent_observer.Wait();
  EXPECT_TRUE(child_detached);

  EXPECT_EQ(nullptr, GetMainFrame()->FirstChild());
}

// Verifies that hiding an <img> via visibility:hidden before OnImageAnnotated
// runs (leaving the AXObject attached with IsIncludedInTree() == false) does
// not trip DCHECK(obj.IsIncludedInTree()) in MarkDirty().
TEST_F(AXImageAnnotatorTest, OnImageAnnotatedImageHiddenBeforeReply) {
  LoadHTMLAndRefreshAccessibilityTree(base::StringPrintf(R"HTML(
      <body>
        <p>Test document</p>
        <img id="A" src="%s" style="width: 200px; height: 150px;">
      </body>
      )HTML",
                                                         kImage1)
                                          .c_str());
  mock_annotator_service().WaitForCalls(1u);
  ASSERT_EQ(1u, mock_annotator_service().callbacks_.size());

  ExecuteJavaScriptForTests(
      "document.getElementById('A').style.visibility = 'hidden';");
  std::move(mock_annotator_service().callbacks_[0])
      .Run(image_annotation::mojom::AnnotateImageResult::NewErrorCode(
          image_annotation::mojom::AnnotateImageError::kFailure));
  SendPendingAccessibilityEvents();

  WebAXObject image_obj = WebAXObject::FromWebNode(
      GetMainFrame()->GetDocument().GetElementById("A"));
  ASSERT_FALSE(image_obj.IsDetached());
  EXPECT_FALSE(image_obj.IsIncludedInTree());
}

}  // namespace content
