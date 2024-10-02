// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/annotations/ax_main_node_annotator.h"

#include "base/test/scoped_feature_list.h"
#include "content/renderer/accessibility/annotations/ax_annotators_manager.h"
#include "content/renderer/accessibility/render_accessibility_impl_test.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/screen_ai/screen_ai_service_impl.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_tree.h"

using blink::WebAXObject;
using blink::WebDocument;

namespace content {

class MockMainNodeAnnotationService
    : public screen_ai::mojom::Screen2xMainContentExtractor {
 public:
  MockMainNodeAnnotationService() = default;
  MockMainNodeAnnotationService(const MockMainNodeAnnotationService&) = delete;
  MockMainNodeAnnotationService& operator=(
      const MockMainNodeAnnotationService&) = delete;
  ~MockMainNodeAnnotationService() override = default;

  mojo::PendingRemote<screen_ai::mojom::Screen2xMainContentExtractor>
  GetRemote() {
    mojo::PendingRemote<screen_ai::mojom::Screen2xMainContentExtractor> remote;
    receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  void ExtractMainContent(const ::ui::AXTreeUpdate& snapshot,
                          int64_t ukm_source_id,
                          ExtractMainContentCallback callback) override {}

  void ExtractMainNode(const ui::AXTreeUpdate& snapshot,
                       ExtractMainNodeCallback callback) override {
    // For the mock, let any node which contains text be a content node.
    for (ui::AXNodeData node : snapshot.nodes) {
      if (node.HasStringAttribute(ax::mojom::StringAttribute::kName) &&
          node.role == ax::mojom::Role::kStaticText) {
        content_nodes_.push_back(node.id);
      }
    }
    ui::AXTree tree(snapshot);
    main_ = screen_ai::ScreenAIService::ComputeMainNodeForTesting(
        &tree, content_nodes_);
    std::move(callback).Run(main_);
  }

  // Tests should not modify entries in these lists.
  std::vector<ui::AXNodeID> content_nodes_;
  ui::AXNodeID main_ = ui::kInvalidAXNodeID;

 private:
  mojo::ReceiverSet<screen_ai::mojom::Screen2xMainContentExtractor> receivers_;
};

class AXMainNodeAnnotatorTest : public RenderAccessibilityImplTest {
 public:
  AXMainNodeAnnotatorTest() = default;
  AXMainNodeAnnotatorTest(const AXMainNodeAnnotatorTest&) = delete;
  AXMainNodeAnnotatorTest& operator=(const AXMainNodeAnnotatorTest&) = delete;
  ~AXMainNodeAnnotatorTest() override = default;

 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kMainNodeAnnotations);
    RenderAccessibilityImplTest::SetUp();
    ui::AXMode mode = ui::kAXModeComplete;
    mode.set_mode(ui::AXMode::kAnnotateMainNode, true);
    SetMode(mode);
    auto annotator =
        std::make_unique<AXMainNodeAnnotator>(GetRenderAccessibilityImpl());
    annotator->BindAnnotatorForTesting(mock_annotator_service().GetRemote());
    GetRenderAccessibilityImpl()
        ->ax_annotators_manager_for_testing()
        ->AddAnnotatorForTesting(std::move(annotator));
    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    GetRenderAccessibilityImpl()
        ->ax_annotators_manager_for_testing()
        ->ClearAnnotatorsForTesting();
    task_environment_.RunUntilIdle();
    RenderAccessibilityImplTest::TearDown();
  }

  MockMainNodeAnnotationService& mock_annotator_service() {
    return mock_annotator_service_;
  }

  void Annotate(const blink::WebDocument& document,
                ui::AXTreeUpdate* update,
                bool load_complete) {
    GetRenderAccessibilityImpl()->ax_annotators_manager_for_testing()->Annotate(
        document, update, load_complete);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  MockMainNodeAnnotationService mock_annotator_service_;
};

TEST_F(AXMainNodeAnnotatorTest, AnnotateMainNode) {
  LoadHTMLAndRefreshAccessibilityTree(R"HTML(
      <body>
        <div>
          <div>Heading</div>
          <div>Paragraph</div>
          <div>Paragraph</div>
        </div>
      </body>
      )HTML");

  // Every time we call a method on a Mojo interface, a message is posted to the
  // current task queue. We need to ask the queue to drain itself before we
  // check test expectations.
  task_environment_.RunUntilIdle();

  EXPECT_EQ(3u, mock_annotator_service().content_nodes_.size());
  EXPECT_NE(ui::kInvalidAXNodeID, mock_annotator_service().main_);

  WebDocument document = GetMainFrame()->GetDocument();
  WebAXObject root = WebAXObject::FromWebDocument(document);
  ASSERT_FALSE(root.IsNull());

  // The expected main node is the div which is the parent of the 3 divs with
  // text.
  WebAXObject expected_main = root.ChildAt(0).ChildAt(0).ChildAt(0);
  ASSERT_FALSE(expected_main.IsNull());

  for (int i = 0; i < 3; i++) {
    ASSERT_EQ(mock_annotator_service().content_nodes_[i],
              expected_main.ChildAt(i).ChildAt(0).AxID());
  }

  // The main node found by the mock annotator should match the id of the
  // expected main.
  ASSERT_EQ(mock_annotator_service().main_, expected_main.AxID());
}

TEST_F(AXMainNodeAnnotatorTest, DoesNothingIfAuthorProvidedNode) {
  LoadHTMLAndRefreshAccessibilityTree(R"HTML(
      <body>
        <div role="main">
          <div>Heading</div>
          <div>Paragraph</div>
          <div>Paragraph</div>
        </div>
      </body>
      )HTML");

  // Expect mock annotator service did not run.
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0u, mock_annotator_service().content_nodes_.size());
  EXPECT_EQ(ui::kInvalidAXNodeID, mock_annotator_service().main_);
}

}  // namespace content
