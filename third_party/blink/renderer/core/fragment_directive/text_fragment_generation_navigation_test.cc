// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "components/shared_highlighting/core/common/shared_highlighting_data_driven_test.h"
#include "components/shared_highlighting/core/common/shared_highlighting_data_driven_test_results.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/annotation/annotation_agent_container_impl.h"
#include "third_party/blink/renderer/core/annotation/annotation_agent_impl.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/fragment_directive/text_directive.h"
#include "third_party/blink/renderer/core/fragment_directive/text_fragment_anchor.h"
#include "third_party/blink/renderer/core/fragment_directive/text_fragment_handler.h"
#include "third_party/blink/renderer/core/fragment_directive/text_fragment_selector_generator.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

using test::RunPendingTasks;

class TextFragmentGenerationNavigationTest
    : public shared_highlighting::SharedHighlightingDataDrivenTest,
      public testing::WithParamInterface<base::FilePath>,
      public SimTest {
 public:
  TextFragmentGenerationNavigationTest() = default;
  ~TextFragmentGenerationNavigationTest() override = default;

  void SetUp() override;

  void RunAsyncMatchingTasks();
  TextFragmentAnchor* GetTextFragmentAnchor();

  // SharedHighlightingDataDrivenTest:
  shared_highlighting::SharedHighlightingDataDrivenTestResults
  GenerateAndNavigate(std::string html_content,
                      std::string* start_parent_id,
                      int start_offset_in_parent,
                      std::optional<int> start_text_offset,
                      std::string* end_parent_id,
                      int end_offset_in_parent,
                      std::optional<int> end_text_offset,
                      std::string selected_text,
                      std::string* highlight_text) override;

  void LoadHTML(String url, String html_content);

  RangeInFlatTree* GetSelectionRange(std::string* start_parent_id,
                                     int start_offset_in_parent,
                                     std::optional<int> start_text_offset,
                                     std::string* end_parent_id,
                                     int end_offset_in_parent,
                                     std::optional<int> end_text_offset);

  String GenerateSelector(const RangeInFlatTree& selection_range);

  // Returns the string that's highlighted. Supports only single highlight in a
  // page.
  String GetHighlightedText();
};

void TextFragmentGenerationNavigationTest::SetUp() {
  SimTest::SetUp();
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 600));
}

void TextFragmentGenerationNavigationTest::RunAsyncMatchingTasks() {
  ThreadScheduler::Current()
      ->ToMainThreadScheduler()
      ->StartIdlePeriodForTesting();
  RunPendingTasks();
}

TextFragmentAnchor*
TextFragmentGenerationNavigationTest::GetTextFragmentAnchor() {
  FragmentAnchor* fragmentAnchor =
      GetDocument().GetFrame()->View()->GetFragmentAnchor();
  if (!fragmentAnchor || !fragmentAnchor->IsTextFragmentAnchor()) {
    return nullptr;
  }
  return static_cast<TextFragmentAnchor*>(fragmentAnchor);
}

void TextFragmentGenerationNavigationTest::LoadHTML(String url,
                                                    String html_content) {
  SimRequest request(url, "text/html");
  LoadURL(url);
  request.Complete(html_content);
}

RangeInFlatTree* TextFragmentGenerationNavigationTest::GetSelectionRange(
    std::string* start_parent_id,
    int start_offset_in_parent,
    std::optional<int> start_text_offset,
    std::string* end_parent_id,
    int end_offset_in_parent,
    std::optional<int> end_text_offset) {
  // Parent of start node will be the node with `start_parent_id` id
  // or the DOM body if no `start_parent_id`.
  Node* start_parent_node = start_parent_id == nullptr
                                ? GetDocument().body()
                                : GetDocument().getElementById(
                                      AtomicString(start_parent_id->c_str()));

  // Parent of end node will be the node with `end_parent_id` id
  // or the DOM body if no `end_parent_id`.
  Node* end_parent_node =
      end_parent_id == nullptr
          ? GetDocument().body()
          : GetDocument().getElementById(AtomicString(end_parent_id->c_str()));

  const Node* start_node =
      start_parent_node->childNodes()->item(start_offset_in_parent);
  const Node* end_node =
      end_parent_node->childNodes()->item(end_offset_in_parent);

  int start_offset = start_text_offset.has_value() ? *start_text_offset : 0;
  int end_offset = end_text_offset.has_value() ? *end_text_offset : 0;

  const auto& selected_start = Position(start_node, start_offset);
  const auto& selected_end = Position(end_node, end_offset);

  return MakeGarbageCollected<RangeInFlatTree>(
      ToPositionInFlatTree(selected_start), ToPositionInFlatTree(selected_end));
}

String TextFragmentGenerationNavigationTest::GenerateSelector(
    const RangeInFlatTree& selection_range) {
  String selector;
  auto lambda = [](String& selector,
                   const TextFragmentSelector& generated_selector,
                   shared_highlighting::LinkGenerationError error) {
    selector = generated_selector.ToString();
  };
  auto callback = WTF::BindOnce(lambda, std::ref(selector));

  MakeGarbageCollected<TextFragmentSelectorGenerator>(GetDocument().GetFrame())
      ->Generate(selection_range, std::move(callback));
  base::RunLoop().RunUntilIdle();
  return selector;
}

String TextFragmentGenerationNavigationTest::GetHighlightedText() {
  auto* container = AnnotationAgentContainerImpl::CreateIfNeeded(GetDocument());
  // Returns a null string, distinguishable from an empty string.
  if (!container)
    return String();

  HeapHashSet<Member<AnnotationAgentImpl>> shared_highlight_agents =
      container->GetAgentsOfType(
          mojom::blink::AnnotationType::kSharedHighlight);
  if (shared_highlight_agents.size() != 1)
    return String();

  AnnotationAgentImpl* agent = *shared_highlight_agents.begin();
  return PlainText(agent->GetAttachedRange().ToEphemeralRange());
}

shared_highlighting::SharedHighlightingDataDrivenTestResults
TextFragmentGenerationNavigationTest::GenerateAndNavigate(
    std::string html_content,
    std::string* start_parent_id,
    int start_offset_in_parent,
    std::optional<int> start_text_offset,
    std::string* end_parent_id,
    int end_offset_in_parent,
    std::optional<int> end_text_offset,
    std::string selected_text,
    std::string* highlight_text) {
  String base_url = "https://example.com/test.html";
  String html_content_wtf = String::FromUTF8(html_content.c_str());
  LoadHTML(base_url, html_content_wtf);

  RangeInFlatTree* selection_range = GetSelectionRange(
      start_parent_id, start_offset_in_parent, start_text_offset, end_parent_id,
      end_offset_in_parent, end_text_offset);

  // Generate text fragment selector.
  String selector = GenerateSelector(*selection_range);

  if (selector.empty()) {
    return shared_highlighting::SharedHighlightingDataDrivenTestResults();
  }

  // Navigate to generated link to text.
  String link_to_text_url = base_url + "#:~:text=" + selector;
  LoadHTML(link_to_text_url, html_content_wtf);

  RunAsyncMatchingTasks();
  Compositor().BeginFrame();

  String actual_highlighted_text = GetDocument().Markers().Markers().size() == 1
                                       ? GetHighlightedText()
                                       : String();

  String expected_highlighted_text =
      highlight_text != nullptr ? String::FromUTF8(highlight_text->c_str())
                                : String();

  return shared_highlighting::SharedHighlightingDataDrivenTestResults{
      .generation_success = true,
      .highlighting_success =
          expected_highlighted_text == actual_highlighted_text};
}

TEST_P(TextFragmentGenerationNavigationTest,
       DataDrivenGenerationAndNavigation) {
  RunOneDataDrivenTest(GetParam(), GetOutputDirectory(),
                       /* kIsExpectedToPass */ true);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    TextFragmentGenerationNavigationTest,
    testing::ValuesIn(
        shared_highlighting::SharedHighlightingDataDrivenTest::GetTestFiles()));

}  // namespace blink
