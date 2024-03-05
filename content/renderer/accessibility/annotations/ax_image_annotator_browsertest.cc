// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/annotations/ax_image_annotator.h"

#include "base/test/scoped_feature_list.h"
#include "content/renderer/accessibility/annotations/ax_annotators_manager.h"
#include "content/renderer/accessibility/render_accessibility_impl_test.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace content {

using blink::WebAXObject;
using blink::WebDocument;
using testing::ElementsAre;

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

  mojo::ReceiverSet<image_annotation::mojom::Annotator> receivers_;
};

class AXImageAnnotatorTest : public RenderAccessibilityImplTest {
 public:
  AXImageAnnotatorTest() = default;
  AXImageAnnotatorTest(const AXImageAnnotatorTest&) = delete;
  AXImageAnnotatorTest& operator=(const AXImageAnnotatorTest&) = delete;
  ~AXImageAnnotatorTest() override = default;

 protected:
  void SetUp() override {
    RenderAccessibilityImplTest::SetUp();
    // TODO(nektar): Add the ability to test the AX action that labels images
    // only once.
    ui::AXMode mode = ui::kAXModeComplete;
    mode.set_mode(ui::AXMode::kLabelImages, true);
    SetMode(mode);
    auto annotator =
        std::make_unique<TestAXImageAnnotator>(GetRenderAccessibilityImpl());
    annotator->BindAnnotatorForTesting(mock_annotator_service().GetRemote());
    GetRenderAccessibilityImpl()
        ->ax_annotators_manager_for_testing()
        ->AddAnnotatorForTesting(std::move(annotator));
    AXImageAnnotator::IgnoreProtocolChecksForTesting();
    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
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
  base::test::ScopedFeatureList scoped_feature_list_;
  MockImageAnnotationService mock_annotator_service_;
};

// TODO(crbug.com/1477047, fuchsia:132924): Reenable test on Fuchsia once
// post-lifecycle serialization is turned on.
#if BUILDFLAG(IS_FUCHSIA)
#define MAYBE_OnImageAdded DISABLED_OnImageAdded
#else
#define MAYBE_OnImageAdded OnImageAdded
#endif
TEST_F(AXImageAnnotatorTest, MAYBE_OnImageAdded) {
  LoadHTMLAndRefreshAccessibilityTree(R"HTML(
      <body>
        <p>Test document</p>
        <img id="A" src="test1.jpg"
            style="width: 200px; height: 150px;">
        <img id="B" src="test2.jpg"
            style="visibility: hidden; width: 200px; height: 150px;">
      </body>
      )HTML");

  // Every time we call a method on a Mojo interface, a message is posted to the
  // current task queue. We need to ask the queue to drain itself before we
  // check test expectations.
  task_environment_.RunUntilIdle();

  EXPECT_THAT(mock_annotator_service().image_ids_, ElementsAre("test1.jpg"));
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
              ElementsAre("test1.jpg", "test2.jpg", "test1.jpg", "test2.jpg"));
  ASSERT_EQ(4u, mock_annotator_service().image_processors_.size());
  EXPECT_TRUE(mock_annotator_service().image_processors_[0].is_bound());
  EXPECT_TRUE(mock_annotator_service().image_processors_[1].is_bound());
  EXPECT_TRUE(mock_annotator_service().image_processors_[2].is_bound());
  EXPECT_TRUE(mock_annotator_service().image_processors_[3].is_bound());
  EXPECT_EQ(4u, mock_annotator_service().callbacks_.size());
}

TEST_F(AXImageAnnotatorTest, OnImageUpdated) {
  LoadHTMLAndRefreshAccessibilityTree(R"HTML(
      <body>
        <p>Test document</p>
        <img id="A" src="test1.jpg"
            style="width: 200px; height: 150px;">
      </body>
      )HTML");

  // Every time we call a method on a Mojo interface, a message is posted to the
  // current task queue. We need to ask the queue to drain itself before we
  // check test expectations.
  task_environment_.RunUntilIdle();

  EXPECT_THAT(mock_annotator_service().image_ids_, ElementsAre("test1.jpg"));
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
              ElementsAre("test1.jpg", "test1.jpg"));
  ASSERT_EQ(2u, mock_annotator_service().image_processors_.size());
  EXPECT_TRUE(mock_annotator_service().image_processors_[0].is_bound());
  EXPECT_TRUE(mock_annotator_service().image_processors_[1].is_bound());
  EXPECT_EQ(2u, mock_annotator_service().callbacks_.size());

  // Update node "A".
  ExecuteJavaScriptForTests("document.querySelector('img').src = 'test2.jpg';");
  SendPendingAccessibilityEvents();

  ClearHandledUpdates();
  // This should update the annotations of all images on the page, including the
  // now updated image src.
  MarkSubtreeDirty(root_obj);
  SendPendingAccessibilityEvents();

  EXPECT_THAT(mock_annotator_service().image_ids_,
              ElementsAre("test1.jpg", "test1.jpg", "test2.jpg"));
  ASSERT_EQ(3u, mock_annotator_service().image_processors_.size());
  EXPECT_TRUE(mock_annotator_service().image_processors_[0].is_bound());
  EXPECT_TRUE(mock_annotator_service().image_processors_[1].is_bound());
  EXPECT_TRUE(mock_annotator_service().image_processors_[2].is_bound());
  EXPECT_EQ(3u, mock_annotator_service().callbacks_.size());
}

}  // namespace content
