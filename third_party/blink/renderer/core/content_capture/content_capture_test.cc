// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/content_capture/content_capture_manager.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/web/web_content_capture_client.h"
#include "third_party/blink/public/web/web_content_holder.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html_element_type_helpers.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

namespace {

gfx::Rect GetRect(LayoutObject* layout_object) {
  return ToEnclosingRect(VisualRectInDocument(*layout_object));
}

void FindNodeVectorsDiff(const Vector<Persistent<Node>>& a,
                         const Vector<Persistent<Node>>& b,
                         Vector<Persistent<Node>>& a_diff_b) {
  for (auto& i : a) {
    if (!b.Contains(i))
      a_diff_b.push_back(i);
  }
}

void FindNodeVectorsDiff(const Vector<Persistent<Node>>& a,
                         const Vector<Persistent<Node>>& b,
                         Vector<Persistent<Node>>& a_diff_b,
                         Vector<Persistent<Node>>& b_diff_a) {
  FindNodeVectorsDiff(a, b, a_diff_b);
  FindNodeVectorsDiff(b, a, b_diff_a);
}

void FindNodeVectorsUnion(const Vector<Persistent<Node>>& a,
                          const Vector<Persistent<Node>>& b,
                          HashSet<Persistent<Node>>& a_and_b) {
  for (auto& n : a) {
    a_and_b.insert(n);
  }
  for (auto& n : b) {
    a_and_b.insert(n);
  }
}

void ToNodeIds(const Vector<Persistent<Node>>& nodes,
               Vector<int64_t>& node_ids) {
  for (auto& v : nodes) {
    node_ids.push_back(reinterpret_cast<int64_t>(static_cast<Node*>(v)));
  }
}

void ToNodeTexts(const Vector<Persistent<Node>>& nodes, Vector<String>& texts) {
  for (auto& n : nodes)
    texts.push_back(n->nodeValue());
}

}  // namespace

class WebContentCaptureClientTestHelper : public WebContentCaptureClient {
 public:
  ~WebContentCaptureClientTestHelper() override = default;

  base::TimeDelta GetTaskInitialDelay() const override {
    return base::Milliseconds(500);
  }

  void DidCaptureContent(const WebVector<WebContentHolder>& data,
                         bool first_data) override {
    data_ = data;
    first_data_ = first_data;
    for (auto& d : data) {
      auto text = d.GetValue();
      all_text_.push_back(text);
      captured_text_.push_back(text);
    }
  }

  void DidUpdateContent(const WebVector<WebContentHolder>& data) override {
    updated_data_ = data;
    for (auto& d : data)
      updated_text_.push_back(d.GetValue());
  }

  void DidRemoveContent(WebVector<int64_t> data) override {
    removed_data_ = data;
  }

  bool FirstData() const { return first_data_; }

  const WebVector<WebContentHolder>& Data() const { return data_; }

  const WebVector<WebContentHolder>& UpdatedData() const {
    return updated_data_;
  }

  const Vector<String>& AllText() const { return all_text_; }

  const Vector<String>& CapturedText() const { return captured_text_; }

  const Vector<String>& UpdatedText() const { return updated_text_; }

  const WebVector<int64_t>& RemovedData() const { return removed_data_; }

  void ResetResults() {
    first_data_ = false;
    data_.clear();
    updated_data_.clear();
    removed_data_.clear();
    captured_text_.clear();
  }

 private:
  bool first_data_ = false;
  WebVector<WebContentHolder> data_;
  WebVector<WebContentHolder> updated_data_;
  WebVector<int64_t> removed_data_;
  Vector<String> all_text_;
  Vector<String> updated_text_;
  Vector<String> captured_text_;
};

class ContentCaptureLocalFrameClientHelper : public EmptyLocalFrameClient {
 public:
  ContentCaptureLocalFrameClientHelper(WebContentCaptureClient& client)
      : client_(client) {}

  WebContentCaptureClient* GetWebContentCaptureClient() const override {
    return &client_;
  }

 private:
  WebContentCaptureClient& client_;
};

class ContentCaptureTest : public PageTestBase,
                           public ::testing::WithParamInterface<
                               std::vector<base::test::FeatureRef>> {
 public:
  ContentCaptureTest()
      : PageTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    EnablePlatform();
    feature_list_.InitWithFeatures(
        GetParam(),
        /*disabled_features=*/std::vector<base::test::FeatureRef>());
  }

  void SetUp() override {
    content_capture_client_ =
        std::make_unique<WebContentCaptureClientTestHelper>();
    local_frame_client_ =
        MakeGarbageCollected<ContentCaptureLocalFrameClientHelper>(
            *content_capture_client_);
    SetupPageWithClients(nullptr, local_frame_client_);
    SetHtmlInnerHTML(
        "<!DOCTYPE HTML>"
        "<p id='p1'>1</p>"
        "<p id='p2'>2</p>"
        "<p id='p3'>3</p>"
        "<p id='p4'>4</p>"
        "<p id='p5'>5</p>"
        "<p id='p6'>6</p>"
        "<p id='p7'>7</p>"
        "<p id='p8'>8</p>"
        "<div id='d1'></div>"
        "<p id='invisible'>invisible</p>");
    InitNodeHolders();
    // Setup captured content to ContentCaptureTask, it isn't necessary once
    // ContentCaptureManager is created by LocalFrame.
    GetOrResetContentCaptureManager()
        ->GetContentCaptureTaskForTesting()
        ->SetCapturedContentForTesting(node_ids_);
    InitScrollingTestData();
  }

  void SimulateScrolling(wtf_size_t step) {
    CHECK_LT(step, 4u);
    GetOrResetContentCaptureManager()
        ->GetContentCaptureTaskForTesting()
        ->SetCapturedContentForTesting(scrolling_node_ids_[step]);
    GetOrResetContentCaptureManager()->OnScrollPositionChanged();
  }

  void CreateTextNodeAndNotifyManager() {
    Document& doc = GetDocument();
    Node* node = doc.createTextNode("New Text");
    Element* element = MakeGarbageCollected<Element>(html_names::kPTag, &doc);
    element->appendChild(node);
    Element* div_element = GetElementById("d1");
    div_element->appendChild(element);
    UpdateAllLifecyclePhasesForTest();
    GetOrResetContentCaptureManager()->ScheduleTaskIfNeeded(*node);
    created_node_id_ = DOMNodeIds::IdForNode(node);
    Vector<cc::NodeInfo> captured_content{
        cc::NodeInfo(created_node_id_, GetRect(node->GetLayoutObject()))};
    GetOrResetContentCaptureManager()
        ->GetContentCaptureTaskForTesting()
        ->SetCapturedContentForTesting(captured_content);
  }

  ContentCaptureManager* GetOrResetContentCaptureManager() {
    if (content_capture_manager_ == nullptr)
      content_capture_manager_ = GetFrame().GetOrResetContentCaptureManager();
    return content_capture_manager_;
  }

  WebContentCaptureClientTestHelper* GetWebContentCaptureClient() const {
    return content_capture_client_.get();
  }

  ContentCaptureTask* GetContentCaptureTask() {
    return GetOrResetContentCaptureManager()->GetContentCaptureTaskForTesting();
  }

  void RunContentCaptureTask() {
    ResetResult();
    FastForwardBy(GetWebContentCaptureClient()->GetTaskInitialDelay());
  }

  void RunNextContentCaptureTask() {
    ResetResult();
    FastForwardBy(
        GetContentCaptureTask()->GetTaskDelayForTesting().GetNextTaskDelay());
  }

  void RemoveNode(Node* node) {
    // Remove the node.
    node->remove();
    GetOrResetContentCaptureManager()->OnLayoutTextWillBeDestroyed(*node);
  }

  void RemoveUnsentNode(const WebVector<WebContentHolder>& sent_nodes) {
    // Find a node isn't in sent_nodes
    for (auto node : nodes_) {
      bool found_in_sent = false;
      for (auto& sent : sent_nodes) {
        found_in_sent = (node->nodeValue().Utf8().c_str() == sent.GetValue());
        if (found_in_sent)
          break;
      }
      if (!found_in_sent) {
        RemoveNode(node);
        return;
      }
    }
    // Didn't find unsent nodes.
    NOTREACHED_IN_MIGRATION();
  }

  size_t GetExpectedFirstResultSize() { return ContentCaptureTask::kBatchSize; }

  size_t GetExpectedSecondResultSize() {
    return node_ids_.size() - GetExpectedFirstResultSize();
  }

  const Vector<cc::NodeInfo>& NodeIds() const { return node_ids_; }
  const Vector<Persistent<Node>> Nodes() const { return nodes_; }

  Node& invisible_node() const { return *invisible_node_; }

  const Vector<Vector<String>>& scrolling_expected_captured_nodes() {
    return scrolling_expected_captured_nodes_;
  }

  const Vector<Vector<int64_t>>& scrolling_expected_removed_nodes() {
    return scrolling_expected_removed_nodes_;
  }

 private:
  void ResetResult() {
    GetWebContentCaptureClient()->ResetResults();
  }

  void BuildNodesInfo(const Vector<String>& ids,
                      Vector<Persistent<Node>>& nodes,
                      Vector<cc::NodeInfo>& node_ids) {
    for (auto id : ids) {
      Node* node = GetDocument().getElementById(AtomicString(id))->firstChild();
      CHECK(node);
      LayoutObject* layout_object = node->GetLayoutObject();
      CHECK(layout_object);
      CHECK(layout_object->IsText());
      nodes.push_back(node);
      GetOrResetContentCaptureManager()->ScheduleTaskIfNeeded(*node);
      node_ids.push_back(
          cc::NodeInfo(node->GetDomNodeId(), GetRect(layout_object)));
    }
  }

  // TODO(michaelbai): Remove this once integrate with LayoutText.
  void InitNodeHolders() {
    BuildNodesInfo(
        Vector<String>{"p1", "p2", "p3", "p4", "p5", "p6", "p7", "p8"}, nodes_,
        node_ids_);
    invisible_node_ = GetElementById("invisible")->firstChild();
    DCHECK(invisible_node_.Get());
  }

  void InitScrollingTestData() {
    Vector<Vector<Persistent<Node>>> nodes{4};
    BuildNodesInfo(Vector<String>{"p1", "p2", "p3"}, nodes[0],
                   scrolling_node_ids_[0]);
    BuildNodesInfo(Vector<String>{"p3", "p4", "p5"}, nodes[1],
                   scrolling_node_ids_[1]);
    BuildNodesInfo(Vector<String>{"p6", "p7", "p8"}, nodes[2],
                   scrolling_node_ids_[2]);
    BuildNodesInfo(Vector<String>{"p2", "p3"}, nodes[3],
                   scrolling_node_ids_[3]);
    // Build expected result.
    if (base::FeatureList::IsEnabled(
            features::kContentCaptureConstantStreaming)) {
      for (int i = 0; i < 4; ++i) {
        Vector<Persistent<Node>> a_diff_b;
        Vector<Persistent<Node>> b_diff_a;
        FindNodeVectorsDiff(nodes[i],
                            i == 0 ? Vector<Persistent<Node>>() : nodes[i - 1],
                            a_diff_b, b_diff_a);
        ToNodeTexts(a_diff_b, scrolling_expected_captured_nodes_[i]);
        ToNodeIds(b_diff_a, scrolling_expected_removed_nodes_[i]);
      }
    } else {
      HashSet<Persistent<Node>> sent;
      for (int i = 0; i < 4; ++i) {
        Vector<Persistent<Node>> a_diff_b;
        Vector<Persistent<Node>> b(sent);
        FindNodeVectorsDiff(nodes[i], b, a_diff_b);
        ToNodeTexts(a_diff_b, scrolling_expected_captured_nodes_[i]);
        sent.clear();
        FindNodeVectorsUnion(b, nodes[i], sent);
      }
    }
  }

  Vector<Persistent<Node>> nodes_;
  Vector<cc::NodeInfo> node_ids_;
  Persistent<Node> invisible_node_;
  Vector<Vector<String>> scrolling_expected_captured_nodes_{4};
  Vector<Vector<int64_t>> scrolling_expected_removed_nodes_{4};
  Vector<Vector<cc::NodeInfo>> scrolling_node_ids_{4};
  std::unique_ptr<WebContentCaptureClientTestHelper> content_capture_client_;
  Persistent<ContentCaptureManager> content_capture_manager_;
  Persistent<ContentCaptureLocalFrameClientHelper> local_frame_client_;
  DOMNodeId created_node_id_ = kInvalidDOMNodeId;
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    ContentCaptureTest,
    testing::Values(std::vector<base::test::FeatureRef>{},
                    std::vector<base::test::FeatureRef>{
                        features::kContentCaptureConstantStreaming}));

TEST_P(ContentCaptureTest, Basic) {
  RunContentCaptureTask();
  EXPECT_EQ(ContentCaptureTask::TaskState::kStop,
            GetContentCaptureTask()->GetTaskStateForTesting());
  EXPECT_FALSE(GetWebContentCaptureClient()->Data().empty());
  EXPECT_EQ(GetExpectedSecondResultSize(),
            GetWebContentCaptureClient()->Data().size());
}

TEST_P(ContentCaptureTest, Scrolling) {
  for (wtf_size_t step = 0; step < 4; ++step) {
    SimulateScrolling(step);
    RunContentCaptureTask();
    EXPECT_EQ(ContentCaptureTask::TaskState::kStop,
              GetContentCaptureTask()->GetTaskStateForTesting());
    EXPECT_THAT(GetWebContentCaptureClient()->CapturedText(),
                testing::UnorderedElementsAreArray(
                    scrolling_expected_captured_nodes()[step]))
        << "at step " << step;
    EXPECT_THAT(GetWebContentCaptureClient()->RemovedData(),
                testing::UnorderedElementsAreArray(
                    scrolling_expected_removed_nodes()[step]))
        << "at step " << step;
  }
}

TEST_P(ContentCaptureTest, PauseAndResume) {
  // The task stops before captures content.
  GetContentCaptureTask()->SetTaskStopForTesting(
      ContentCaptureTask::TaskState::kCaptureContent);
  RunContentCaptureTask();
  EXPECT_FALSE(GetWebContentCaptureClient()->FirstData());
  EXPECT_TRUE(GetWebContentCaptureClient()->Data().empty());
  EXPECT_TRUE(GetWebContentCaptureClient()->RemovedData().empty());

  // The task stops before sends the captured content out.
  GetContentCaptureTask()->SetTaskStopForTesting(
      ContentCaptureTask::TaskState::kProcessCurrentSession);
  RunContentCaptureTask();
  EXPECT_FALSE(GetWebContentCaptureClient()->FirstData());
  EXPECT_TRUE(GetWebContentCaptureClient()->Data().empty());
  EXPECT_TRUE(GetWebContentCaptureClient()->RemovedData().empty());

  // The task should be stop at kProcessRetryTask because the captured content
  // needs to be sent with 2 batch.
  GetContentCaptureTask()->SetTaskStopForTesting(
      ContentCaptureTask::TaskState::kProcessRetryTask);
  RunContentCaptureTask();
  EXPECT_TRUE(GetWebContentCaptureClient()->FirstData());
  EXPECT_FALSE(GetWebContentCaptureClient()->Data().empty());
  EXPECT_TRUE(GetWebContentCaptureClient()->RemovedData().empty());
  EXPECT_EQ(GetExpectedFirstResultSize(),
            GetWebContentCaptureClient()->Data().size());

  // Run task until it stops, task will not capture content, because there is no
  // content change, so we have 3 NodeHolders.
  GetContentCaptureTask()->SetTaskStopForTesting(
      ContentCaptureTask::TaskState::kStop);
  RunContentCaptureTask();
  EXPECT_FALSE(GetWebContentCaptureClient()->FirstData());
  EXPECT_FALSE(GetWebContentCaptureClient()->Data().empty());
  EXPECT_TRUE(GetWebContentCaptureClient()->RemovedData().empty());
  EXPECT_EQ(GetExpectedSecondResultSize(),
            GetWebContentCaptureClient()->Data().size());
}

TEST_P(ContentCaptureTest, NodeOnlySendOnce) {
  // Send all nodes
  RunContentCaptureTask();
  EXPECT_FALSE(GetWebContentCaptureClient()->Data().empty());
  EXPECT_EQ(GetExpectedSecondResultSize(),
            GetWebContentCaptureClient()->Data().size());

  GetOrResetContentCaptureManager()->OnScrollPositionChanged();
  RunContentCaptureTask();
  EXPECT_TRUE(GetWebContentCaptureClient()->Data().empty());
  EXPECT_TRUE(GetWebContentCaptureClient()->RemovedData().empty());
}

TEST_P(ContentCaptureTest, UnsentNode) {
  // Send all nodes expect |invisible_node_|.
  RunContentCaptureTask();
  EXPECT_FALSE(GetWebContentCaptureClient()->Data().empty());
  EXPECT_EQ(GetExpectedSecondResultSize(),
            GetWebContentCaptureClient()->Data().size());

  // Simulates the |invisible_node_| being changed, and verifies no content
  // change because |invisible_node_| wasn't captured.
  GetOrResetContentCaptureManager()->OnNodeTextChanged(invisible_node());
  RunContentCaptureTask();
  EXPECT_TRUE(GetWebContentCaptureClient()->Data().empty());
  EXPECT_TRUE(GetWebContentCaptureClient()->UpdatedData().empty());
  EXPECT_TRUE(GetWebContentCaptureClient()->RemovedData().empty());

  // Simulates the |invisible_node_| being removed, and verifies no content
  // change because |invisible_node_| wasn't captured.
  GetOrResetContentCaptureManager()->OnLayoutTextWillBeDestroyed(
      invisible_node());
  RunContentCaptureTask();
  EXPECT_TRUE(GetWebContentCaptureClient()->Data().empty());
  EXPECT_TRUE(GetWebContentCaptureClient()->UpdatedData().empty());
  EXPECT_TRUE(GetWebContentCaptureClient()->RemovedData().empty());
}

TEST_P(ContentCaptureTest, RemoveNodeBeforeSendingOut) {
  // Capture the content, but didn't send them.
  GetContentCaptureTask()->SetTaskStopForTesting(
      ContentCaptureTask::TaskState::kProcessCurrentSession);
  RunContentCaptureTask();
  EXPECT_TRUE(GetWebContentCaptureClient()->Data().empty());

  // Remove the node and sent the captured content out.
  RemoveNode(Nodes().at(0));
  GetContentCaptureTask()->SetTaskStopForTesting(
      ContentCaptureTask::TaskState::kProcessRetryTask);
  RunContentCaptureTask();
  EXPECT_EQ(GetExpectedFirstResultSize(),
            GetWebContentCaptureClient()->Data().size());
  EXPECT_EQ(0u, GetWebContentCaptureClient()->RemovedData().size());
  RunContentCaptureTask();
  // Total 7 content returned instead of 8.
  EXPECT_EQ(GetExpectedSecondResultSize() - 1,
            GetWebContentCaptureClient()->Data().size());
  EXPECT_EQ(0u, GetWebContentCaptureClient()->RemovedData().size());
  RunContentCaptureTask();
  // No removed node because it hasn't been sent out.
  EXPECT_EQ(0u, GetWebContentCaptureClient()->Data().size());
  EXPECT_EQ(0u, GetWebContentCaptureClient()->RemovedData().size());
}

TEST_P(ContentCaptureTest, RemoveNodeInBetweenSendingOut) {
  // Capture the content, but didn't send them.
  GetContentCaptureTask()->SetTaskStopForTesting(
      ContentCaptureTask::TaskState::kProcessCurrentSession);
  RunContentCaptureTask();
  EXPECT_TRUE(GetWebContentCaptureClient()->Data().empty());

  // Sends first batch.
  GetContentCaptureTask()->SetTaskStopForTesting(
      ContentCaptureTask::TaskState::kProcessRetryTask);
  RunContentCaptureTask();
  EXPECT_EQ(GetExpectedFirstResultSize(),
            GetWebContentCaptureClient()->Data().size());
  EXPECT_EQ(0u, GetWebContentCaptureClient()->RemovedData().size());

  // This relies on each node to have different value.
  RemoveUnsentNode(GetWebContentCaptureClient()->Data());
  GetContentCaptureTask()->SetTaskStopForTesting(
      ContentCaptureTask::TaskState::kProcessRetryTask);
  RunContentCaptureTask();
  // Total 7 content returned instead of 8.
  EXPECT_EQ(GetExpectedSecondResultSize() - 1,
            GetWebContentCaptureClient()->Data().size());
  EXPECT_EQ(0u, GetWebContentCaptureClient()->RemovedData().size());
  RunContentCaptureTask();
  // No removed node because it hasn't been sent out.
  EXPECT_EQ(0u, GetWebContentCaptureClient()->Data().size());
  EXPECT_EQ(0u, GetWebContentCaptureClient()->RemovedData().size());
}

TEST_P(ContentCaptureTest, RemoveNodeAfterSendingOut) {
  // Captures the content, but didn't send them.
  GetContentCaptureTask()->SetTaskStopForTesting(
      ContentCaptureTask::TaskState::kProcessCurrentSession);
  RunContentCaptureTask();
  EXPECT_TRUE(GetWebContentCaptureClient()->Data().empty());

  // Sends first batch.
  GetContentCaptureTask()->SetTaskStopForTesting(
      ContentCaptureTask::TaskState::kProcessRetryTask);
  RunContentCaptureTask();
  EXPECT_EQ(GetExpectedFirstResultSize(),
            GetWebContentCaptureClient()->Data().size());
  EXPECT_EQ(0u, GetWebContentCaptureClient()->RemovedData().size());

  // Sends second batch.
  RunContentCaptureTask();
  EXPECT_EQ(GetExpectedSecondResultSize(),
            GetWebContentCaptureClient()->Data().size());
  EXPECT_EQ(0u, GetWebContentCaptureClient()->RemovedData().size());

  // Remove the node.
  RemoveNode(Nodes().at(0));
  RunContentCaptureTask();
  EXPECT_EQ(0u, GetWebContentCaptureClient()->Data().size());
  EXPECT_EQ(1u, GetWebContentCaptureClient()->RemovedData().size());
}

TEST_P(ContentCaptureTest, TaskHistogramReporter) {
  // This performs gc for all DocumentSession, flushes the existing
  // SentContentCount and give a clean baseline for histograms.
  // We are not sure if it always work, maybe still be the source of flaky.
  ThreadState::Current()->CollectAllGarbageForTesting();
  base::HistogramTester histograms;

  // The task stops before captures content.
  GetContentCaptureTask()->SetTaskStopForTesting(
      ContentCaptureTask::TaskState::kCaptureContent);
  RunContentCaptureTask();
  // Verify no histogram reported yet.
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kCaptureContentTime, 0u);
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kSendContentTime, 0u);
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kCaptureContentDelayTime, 0u);
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kSentContentCount, 0u);
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kTaskRunsPerCapture, 0u);
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kTaskDelayInMs, 1u);

  // The task stops before sends the captured content out.
  GetContentCaptureTask()->SetTaskStopForTesting(
      ContentCaptureTask::TaskState::kProcessCurrentSession);
  RunContentCaptureTask();
  // Verify has one CaptureContentTime record.
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kCaptureContentTime, 1u);
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kSendContentTime, 0u);
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kCaptureContentDelayTime, 0u);
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kSentContentCount, 0u);

  // The task stops at kProcessRetryTask because the captured content
  // needs to be sent with 2 batch.
  GetContentCaptureTask()->SetTaskStopForTesting(
      ContentCaptureTask::TaskState::kProcessRetryTask);
  RunContentCaptureTask();
  // Verify has one CaptureContentTime, one SendContentTime and one
  // CaptureContentDelayTime record.
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kCaptureContentTime, 1u);
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kSendContentTime, 1u);
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kCaptureContentDelayTime, 1u);
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kSentContentCount, 0u);

  // Run task until it stops, task will not capture content, because there is no
  // content change.
  GetContentCaptureTask()->SetTaskStopForTesting(
      ContentCaptureTask::TaskState::kStop);
  RunContentCaptureTask();
  // Verify has two SendContentTime records.
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kCaptureContentTime, 1u);
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kSendContentTime, 2u);
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kCaptureContentDelayTime, 1u);
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kSentContentCount, 0u);

  // Verify retry task won't count to TaskDelay metrics.
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kTaskDelayInMs, 1u);

  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kTaskRunsPerCapture, 1u);
  // Verify the task ran 4 times, first run stopped before capturing content
  // and 2nd run captured content, 3rd and 4th run sent the content out.
  histograms.ExpectBucketCount(
      ContentCaptureTaskHistogramReporter::kTaskRunsPerCapture, 4u, 1u);

  // Create a node and run task until it stops.
  CreateTextNodeAndNotifyManager();
  GetContentCaptureTask()->SetTaskStopForTesting(
      ContentCaptureTask::TaskState::kStop);
  RunNextContentCaptureTask();
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kCaptureContentTime, 2u);
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kSendContentTime, 3u);
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kCaptureContentDelayTime, 2u);
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kSentContentCount, 0u);

  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kTaskRunsPerCapture, 2u);
  // Verify the task ran 1 times for this session because we didn't explicitly
  // stop it.
  histograms.ExpectBucketCount(
      ContentCaptureTaskHistogramReporter::kTaskRunsPerCapture, 1u, 1u);

  GetContentCaptureTask()->ClearDocumentSessionsForTesting();
  ThreadState::Current()->CollectAllGarbageForTesting();
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kCaptureContentTime, 2u);
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kSendContentTime, 3u);
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kCaptureContentDelayTime, 2u);
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kSentContentCount, 1u);
  // Verify total content has been sent.
  histograms.ExpectBucketCount(
      ContentCaptureTaskHistogramReporter::kSentContentCount, 9u, 1u);

  // Verify TaskDelay was recorded again for node change.
  histograms.ExpectTotalCount(
      ContentCaptureTaskHistogramReporter::kTaskDelayInMs, 2u);
}

TEST_P(ContentCaptureTest, RescheduleTask) {
  // This test assumes test runs much faster than task's long delay which is 5s.
  Persistent<ContentCaptureTask> task = GetContentCaptureTask();
  task->CancelTaskForTesting();
  EXPECT_TRUE(task->GetTaskNextFireIntervalForTesting().is_zero());
  task->Schedule(
      ContentCaptureTask::ScheduleReason::kNonUserActivatedContentChange);
  base::TimeDelta interval1 = task->GetTaskNextFireIntervalForTesting();
  task->Schedule(ContentCaptureTask::ScheduleReason::kScrolling);
  base::TimeDelta interval2 = task->GetTaskNextFireIntervalForTesting();
  EXPECT_LE(interval1, GetWebContentCaptureClient()->GetTaskInitialDelay());
  EXPECT_LE(interval2, interval1);
}

TEST_P(ContentCaptureTest, NotRescheduleTask) {
  // This test assumes test runs much faster than task's long delay which is 5s.
  Persistent<ContentCaptureTask> task = GetContentCaptureTask();
  task->CancelTaskForTesting();
  EXPECT_TRUE(task->GetTaskNextFireIntervalForTesting().is_zero());
  task->Schedule(
      ContentCaptureTask::ScheduleReason::kNonUserActivatedContentChange);
  auto begin = base::TimeTicks::Now();
  base::TimeDelta interval1 = task->GetTaskNextFireIntervalForTesting();
  task->Schedule(
      ContentCaptureTask::ScheduleReason::kNonUserActivatedContentChange);
  base::TimeDelta interval2 = task->GetTaskNextFireIntervalForTesting();
  auto test_running_time = base::TimeTicks::Now() - begin;
  EXPECT_GE(interval1, interval2);
  EXPECT_LE(interval1 - test_running_time, interval2);
}

// TODO(michaelbai): use RenderingTest instead of PageTestBase for multiple
// frame test.
class ContentCaptureSimTest : public SimTest {
 public:
  static const char* kEditableContent;

  ContentCaptureSimTest() : client_(), child_client_() {}
  void SetUp() override {
    SimTest::SetUp();
    MainFrame().SetContentCaptureClient(&client_);
    SetupPage();
  }

  void RunContentCaptureTaskUntil(ContentCaptureTask::TaskState state) {
    Client().ResetResults();
    ChildClient().ResetResults();
    GetDocument()
        .GetFrame()
        ->LocalFrameRoot()
        .GetOrResetContentCaptureManager()
        ->GetContentCaptureTaskForTesting()
        ->RunTaskForTestingUntil(state);
    // Cancels the scheduled task to simulate that the task is running by
    // scheduler.
    GetOrResetContentCaptureManager()
        ->GetContentCaptureTaskForTesting()
        ->CancelTaskForTesting();
  }

  WebContentCaptureClientTestHelper& Client() { return client_; }
  WebContentCaptureClientTestHelper& ChildClient() { return child_client_; }

  enum class ContentType { kAll, kMainFrame, kChildFrame };
  void SetCapturedContent(ContentType type) {
    if (type == ContentType::kMainFrame) {
      SetCapturedContent(main_frame_content_);
    } else if (type == ContentType::kChildFrame) {
      SetCapturedContent(child_frame_content_);
    } else if (type == ContentType::kAll) {
      Vector<cc::NodeInfo> holders(main_frame_content_);
      holders.AppendRange(child_frame_content_.begin(),
                          child_frame_content_.end());
      SetCapturedContent(holders);
    }
  }

  void AddOneNodeToMainFrame() {
    AddNodeToDocument(GetDocument(), main_frame_content_);
    main_frame_expected_text_.push_back("New Text");
  }

  void AddOneNodeToChildFrame() {
    AddNodeToDocument(*child_document_, child_frame_content_);
    child_frame_expected_text_.push_back("New Text");
  }

  void InsertMainFrameEditableContent(const String& content, unsigned offset) {
    InsertNodeContent(GetDocument(), "editable_id", content, offset);
  }

  void DeleteMainFrameEditableContent(unsigned offset, unsigned length) {
    DeleteNodeContent(GetDocument(), "editable_id", offset, length);
  }

  const Vector<String>& MainFrameExpectedText() const {
    return main_frame_expected_text_;
  }

  const Vector<String>& ChildFrameExpectedText() const {
    return child_frame_expected_text_;
  }

  void ReplaceMainFrameExpectedText(const String& old_text,
                                    const String& new_text) {
    std::replace(main_frame_expected_text_.begin(),
                 main_frame_expected_text_.end(), old_text, new_text);
  }

  ContentCaptureManager* GetOrResetContentCaptureManager() {
    return DynamicTo<LocalFrame>(LocalFrameRoot().GetFrame())
        ->GetOrResetContentCaptureManager();
  }

  void SimulateUserInputOnMainFrame() {
    GetOrResetContentCaptureManager()->NotifyInputEvent(
        WebInputEvent::Type::kMouseDown,
        *DynamicTo<LocalFrame>(MainFrame().GetFrame()));
  }

  void SimulateUserInputOnChildFrame() {
    GetOrResetContentCaptureManager()->NotifyInputEvent(
        WebInputEvent::Type::kMouseDown, *child_document_->GetFrame());
  }

  base::TimeDelta GetNextTaskDelay() {
    return GetOrResetContentCaptureManager()
        ->GetContentCaptureTaskForTesting()
        ->GetTaskDelayForTesting()
        .GetNextTaskDelay();
  }

  base::TimeDelta GetTaskNextFireInterval() {
    return GetOrResetContentCaptureManager()
        ->GetContentCaptureTaskForTesting()
        ->GetTaskNextFireIntervalForTesting();
  }

 private:
  void SetupPage() {
    SimRequest main_resource("https://example.com/test.html", "text/html");
    SimRequest frame_resource("https://example.com/frame.html", "text/html");
    LoadURL("https://example.com/test.html");
    WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 6000));
    main_resource.Complete(R"HTML(
      <!DOCTYPE html>
      <body style='background: white'>
      <iframe id=frame name='frame' src=frame.html></iframe>
      <p id='p1'>Hello World1</p>
      <p id='p2'>Hello World2</p>
      <p id='p3'>Hello World3</p>
      <p id='p4'>Hello World4</p>
      <p id='p5'>Hello World5</p>
      <p id='p6'>Hello World6</p>
      <p id='p7'>Hello World7</p>
      <div id='editable_id'>editable</div>
      <svg>
      <text id="s8">Hello World8</text>
      </svg>
      <div id='d1'></div>
      )HTML");
    auto frame1 = Compositor().BeginFrame();
    frame_resource.Complete(R"HTML(
      <!DOCTYPE html>
      <p id='c1'>Hello World11</p>
      <p id='c2'>Hello World12</p>
      <div id='d1'></div>
      )HTML");

    static_cast<WebLocalFrame*>(MainFrame().FindFrameByName("frame"))
        ->SetContentCaptureClient(&child_client_);
    auto* child_frame_element = To<HTMLIFrameElement>(
        GetDocument().getElementById(AtomicString("frame")));
    child_document_ = child_frame_element->contentDocument();
    child_document_->UpdateStyleAndLayout(DocumentUpdateReason::kTest);
    Compositor().BeginFrame();
    InitMainFrameNodeHolders();
    InitChildFrameNodeHolders(*child_document_);
  }

  void InitMainFrameNodeHolders() {
    Vector<String> ids = {"p1", "p2", "p3", "p4",         "p5",
                          "p6", "p7", "s8", "editable_id"};
    main_frame_expected_text_ = {
        "Hello World1", "Hello World2", "Hello World3",
        "Hello World4", "Hello World5", "Hello World6",
        "Hello World7", "Hello World8", kEditableContent};
    InitNodeHolders(main_frame_content_, ids, GetDocument());
    EXPECT_EQ(9u, main_frame_content_.size());
  }

  void InitChildFrameNodeHolders(const Document& doc) {
    Vector<String> ids = {"c1", "c2"};
    child_frame_expected_text_ = {"Hello World11", "Hello World12"};
    InitNodeHolders(child_frame_content_, ids, doc);
    EXPECT_EQ(2u, child_frame_content_.size());
  }

  void InitNodeHolders(Vector<cc::NodeInfo>& buffer,
                       const Vector<String>& ids,
                       const Document& document) {
    for (auto id : ids) {
      auto* layout_object = document.getElementById(AtomicString(id))
                                ->firstChild()
                                ->GetLayoutObject();
      auto* layout_text = To<LayoutText>(layout_object);
      EXPECT_TRUE(layout_text->HasNodeId());
      buffer.push_back(
          cc::NodeInfo(layout_text->EnsureNodeId(), GetRect(layout_object)));
    }
  }

  void AddNodeToDocument(Document& doc, Vector<cc::NodeInfo>& buffer) {
    Node* node = doc.createTextNode("New Text");
    auto* element = MakeGarbageCollected<Element>(html_names::kPTag, &doc);
    element->appendChild(node);
    Element* div_element = doc.getElementById(AtomicString("d1"));
    div_element->appendChild(element);
    Compositor().BeginFrame();
    auto* layout_text = To<LayoutText>(node->GetLayoutObject());
    EXPECT_TRUE(layout_text->HasNodeId());
    buffer.push_back(cc::NodeInfo(layout_text->EnsureNodeId(),
                                  GetRect(node->GetLayoutObject())));
  }

  void InsertNodeContent(Document& doc,
                         const String& id,
                         const String& content,
                         unsigned offset) {
    To<Text>(doc.getElementById(AtomicString(id))->firstChild())
        ->insertData(offset, content, IGNORE_EXCEPTION_FOR_TESTING);
    Compositor().BeginFrame();
  }

  void DeleteNodeContent(Document& doc,
                         const String& id,
                         unsigned offset,
                         unsigned length) {
    To<Text>(doc.getElementById(AtomicString(id))->firstChild())
        ->deleteData(offset, length, IGNORE_EXCEPTION_FOR_TESTING);
    Compositor().BeginFrame();
  }

  void SetCapturedContent(const Vector<cc::NodeInfo>& captured_content) {
    GetDocument()
        .GetFrame()
        ->LocalFrameRoot()
        .GetOrResetContentCaptureManager()
        ->GetContentCaptureTaskForTesting()
        ->SetCapturedContentForTesting(captured_content);
  }

  Vector<String> main_frame_expected_text_;
  Vector<String> child_frame_expected_text_;
  Vector<cc::NodeInfo> main_frame_content_;
  Vector<cc::NodeInfo> child_frame_content_;
  WebContentCaptureClientTestHelper client_;
  WebContentCaptureClientTestHelper child_client_;
  Persistent<Document> child_document_;
};

const char* ContentCaptureSimTest::kEditableContent = "editable";

TEST_F(ContentCaptureSimTest, MultiFrame) {
  SetCapturedContent(ContentType::kAll);
  RunContentCaptureTaskUntil(ContentCaptureTask::TaskState::kStop);
  EXPECT_EQ(4u, Client().Data().size());
  EXPECT_EQ(2u, ChildClient().Data().size());
  EXPECT_THAT(Client().AllText(),
              testing::UnorderedElementsAreArray(MainFrameExpectedText()));
  EXPECT_THAT(ChildClient().AllText(),
              testing::UnorderedElementsAreArray(ChildFrameExpectedText()));
}

TEST_F(ContentCaptureSimTest, AddNodeToMultiFrame) {
  SetCapturedContent(ContentType::kMainFrame);
  // Stops after capturing content.
  RunContentCaptureTaskUntil(
      ContentCaptureTask::TaskState::kProcessCurrentSession);
  EXPECT_TRUE(Client().Data().empty());
  EXPECT_FALSE(Client().FirstData());
  EXPECT_TRUE(ChildClient().Data().empty());

  // Sends the first batch data.
  RunContentCaptureTaskUntil(ContentCaptureTask::TaskState::kProcessRetryTask);
  EXPECT_EQ(5u, Client().Data().size());
  EXPECT_TRUE(Client().FirstData());
  EXPECT_TRUE(ChildClient().Data().empty());

  // Sends the reset of data
  RunContentCaptureTaskUntil(ContentCaptureTask::TaskState::kProcessRetryTask);
  EXPECT_EQ(4u, Client().Data().size());
  EXPECT_FALSE(Client().FirstData());
  EXPECT_TRUE(ChildClient().Data().empty());
  EXPECT_THAT(Client().AllText(),
              testing::UnorderedElementsAreArray(MainFrameExpectedText()));

  AddOneNodeToMainFrame();
  SetCapturedContent(ContentType::kMainFrame);
  RunContentCaptureTaskUntil(ContentCaptureTask::TaskState::kStop);
  // Though returns all main frame content, only new added node is unsent.
  EXPECT_EQ(1u, Client().Data().size());
  EXPECT_FALSE(Client().FirstData());
  EXPECT_TRUE(ChildClient().Data().empty());
  EXPECT_THAT(Client().AllText(),
              testing::UnorderedElementsAreArray(MainFrameExpectedText()));

  AddOneNodeToChildFrame();
  SetCapturedContent(ContentType::kChildFrame);
  RunContentCaptureTaskUntil(ContentCaptureTask::TaskState::kStop);
  EXPECT_EQ(3u, ChildClient().Data().size());
  EXPECT_THAT(ChildClient().AllText(),
              testing::UnorderedElementsAreArray(ChildFrameExpectedText()));
  EXPECT_TRUE(ChildClient().FirstData());
}

TEST_F(ContentCaptureSimTest, ChangeNode) {
  SetCapturedContent(ContentType::kMainFrame);
  RunContentCaptureTaskUntil(ContentCaptureTask::TaskState::kStop);
  EXPECT_EQ(4u, Client().Data().size());
  EXPECT_FALSE(Client().FirstData());
  EXPECT_TRUE(ChildClient().Data().empty());
  EXPECT_THAT(Client().AllText(),
              testing::UnorderedElementsAreArray(MainFrameExpectedText()));
  Vector<String> expected_text_update;
  String insert_text = "content ";

  // Changed content to 'content editable'.
  InsertMainFrameEditableContent(insert_text, 0);
  SetCapturedContent(ContentType::kMainFrame);
  RunContentCaptureTaskUntil(ContentCaptureTask::TaskState::kStop);
  EXPECT_EQ(1u, Client().UpdatedData().size());
  EXPECT_FALSE(Client().FirstData());
  EXPECT_TRUE(ChildClient().Data().empty());
  expected_text_update.push_back(insert_text + kEditableContent);
  EXPECT_THAT(Client().UpdatedText(),
              testing::UnorderedElementsAreArray(expected_text_update));

  // Changing content multiple times before capturing.
  String insert_text1 = "i";
  // Changed content to 'content ieditable'.
  InsertMainFrameEditableContent(insert_text1, insert_text.length());
  String insert_text2 = "s ";
  // Changed content to 'content is editable'.
  InsertMainFrameEditableContent(insert_text2,
                                 insert_text.length() + insert_text1.length());

  SetCapturedContent(ContentType::kMainFrame);
  RunContentCaptureTaskUntil(ContentCaptureTask::TaskState::kStop);
  EXPECT_EQ(1u, Client().UpdatedData().size());
  EXPECT_FALSE(Client().FirstData());
  EXPECT_TRUE(ChildClient().Data().empty());
  expected_text_update.push_back(insert_text + insert_text1 + insert_text2 +
                                 kEditableContent);
  EXPECT_THAT(Client().UpdatedText(),
              testing::UnorderedElementsAreArray(expected_text_update));
}

TEST_F(ContentCaptureSimTest, ChangeNodeBeforeCapture) {
  // Changed content to 'content editable' before capture.
  String insert_text = "content ";
  InsertMainFrameEditableContent(insert_text, 0);
  // Changing content multiple times before capturing.
  String insert_text1 = "i";
  // Changed content to 'content ieditable'.
  InsertMainFrameEditableContent(insert_text1, insert_text.length());
  String insert_text2 = "s ";
  // Changed content to 'content is editable'.
  InsertMainFrameEditableContent(insert_text2,
                                 insert_text.length() + insert_text1.length());

  // The changed content shall be captured as new content.
  ReplaceMainFrameExpectedText(
      kEditableContent,
      insert_text + insert_text1 + insert_text2 + kEditableContent);
  SetCapturedContent(ContentType::kMainFrame);
  RunContentCaptureTaskUntil(ContentCaptureTask::TaskState::kStop);
  EXPECT_EQ(4u, Client().Data().size());
  EXPECT_FALSE(Client().FirstData());
  EXPECT_TRUE(ChildClient().Data().empty());
  EXPECT_TRUE(ChildClient().UpdatedData().empty());
  EXPECT_THAT(Client().AllText(),
              testing::UnorderedElementsAreArray(MainFrameExpectedText()));
}

TEST_F(ContentCaptureSimTest, DeleteNodeContent) {
  SetCapturedContent(ContentType::kMainFrame);
  RunContentCaptureTaskUntil(ContentCaptureTask::TaskState::kStop);
  EXPECT_EQ(4u, Client().Data().size());
  EXPECT_FALSE(Client().FirstData());
  EXPECT_TRUE(ChildClient().Data().empty());
  EXPECT_THAT(Client().AllText(),
              testing::UnorderedElementsAreArray(MainFrameExpectedText()));

  // Deleted 4 char, changed content to 'edit'.
  DeleteMainFrameEditableContent(4, 4);
  SetCapturedContent(ContentType::kMainFrame);
  RunContentCaptureTaskUntil(ContentCaptureTask::TaskState::kStop);
  EXPECT_EQ(1u, Client().UpdatedData().size());
  EXPECT_FALSE(Client().FirstData());
  EXPECT_TRUE(ChildClient().Data().empty());
  Vector<String> expected_text_update;
  expected_text_update.push_back("edit");
  EXPECT_THAT(Client().UpdatedText(),
              testing::UnorderedElementsAreArray(expected_text_update));

  // Emptied content, the node shall be removed.
  DeleteMainFrameEditableContent(0, 4);
  SetCapturedContent(ContentType::kMainFrame);
  RunContentCaptureTaskUntil(ContentCaptureTask::TaskState::kStop);
  EXPECT_TRUE(Client().UpdatedData().empty());
  EXPECT_FALSE(Client().FirstData());
  EXPECT_TRUE(ChildClient().Data().empty());
  EXPECT_EQ(1u, Client().RemovedData().size());
}

TEST_F(ContentCaptureSimTest, UserActivatedDelay) {
  base::TimeDelta expected_delays[] = {
      base::Milliseconds(500), base::Seconds(1),  base::Seconds(2),
      base::Seconds(4),        base::Seconds(8),  base::Seconds(16),
      base::Seconds(32),       base::Seconds(64), base::Seconds(128)};
  size_t expected_delays_size = std::size(expected_delays);
  // The first task has been scheduled but not run yet, the delay will be
  // increased until current task starts to run. Verifies the value is
  // unchanged.
  EXPECT_EQ(expected_delays[0], GetNextTaskDelay());
  // Settles the initial task.
  RunContentCaptureTaskUntil(ContentCaptureTask::TaskState::kStop);

  for (size_t i = 1; i < expected_delays_size; ++i) {
    EXPECT_EQ(expected_delays[i], GetNextTaskDelay());
    // Add a node to schedule the task.
    AddOneNodeToMainFrame();
    auto scheduled_interval = GetTaskNextFireInterval();
    EXPECT_GE(expected_delays[i], scheduled_interval);
    EXPECT_LT(expected_delays[i - 1], scheduled_interval);
    RunContentCaptureTaskUntil(ContentCaptureTask::TaskState::kStop);
  }

  // Verifies the delay is up to 128s.
  AddOneNodeToMainFrame();
  EXPECT_EQ(expected_delays[expected_delays_size - 1], GetNextTaskDelay());
  auto scheduled_interval = GetTaskNextFireInterval();
  EXPECT_GE(expected_delays[expected_delays_size - 1], scheduled_interval);
  EXPECT_LT(expected_delays[expected_delays_size - 2], scheduled_interval);
  RunContentCaptureTaskUntil(ContentCaptureTask::TaskState::kStop);

  // Verifies the user activated change will reset the delay.
  SimulateUserInputOnMainFrame();
  AddOneNodeToMainFrame();
  EXPECT_EQ(expected_delays[0], GetNextTaskDelay());
  scheduled_interval = GetTaskNextFireInterval();
  EXPECT_GE(expected_delays[0], scheduled_interval);

  // Verifies the multiple changes won't reschedule the task.
  AddOneNodeToMainFrame();
  EXPECT_GE(scheduled_interval, GetTaskNextFireInterval());

  RunContentCaptureTaskUntil(ContentCaptureTask::TaskState::kStop);

  // Overrides the main frame's user activation, and verify the child one won't
  // effect the main frame.
  SimulateUserInputOnChildFrame();
  AddOneNodeToMainFrame();
  // Verifies the delay time become one step longer since no user activation was
  // override by child frame's.
  EXPECT_EQ(expected_delays[1], GetNextTaskDelay());
  scheduled_interval = GetTaskNextFireInterval();
  EXPECT_GE(expected_delays[1], scheduled_interval);
  EXPECT_LT(expected_delays[0], scheduled_interval);
}

}  // namespace blink
