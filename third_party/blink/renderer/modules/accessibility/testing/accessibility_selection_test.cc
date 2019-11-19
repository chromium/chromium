// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/testing/accessibility_selection_test.h"

#include <algorithm>
#include <iterator>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/dom/character_data.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/accessibility/ax_position.h"
#include "third_party/blink/renderer/modules/accessibility/ax_selection.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
namespace test {

namespace {

constexpr char kSelectionTestsRelativePath[] = "selection/";
constexpr char kTestFileSuffix[] = ".html";
constexpr char kAXTestExpectationSuffix[] = "-ax.txt";

// Serialize accessibility subtree to selection text.
// Adds a '^' at the selection anchor offset and a '|' at the focus offset.
class AXSelectionSerializer final {
  STACK_ALLOCATED();

 public:
  explicit AXSelectionSerializer(const AXSelection& selection)
      : tree_level_(0), selection_(selection) {}
  ~AXSelectionSerializer() = default;

  std::string Serialize(const AXObject& subtree) {
    if (!selection_.IsValid())
      return {};
    SerializeSubtree(subtree);
    DCHECK_EQ(tree_level_, 0);
    return builder_.ToString().Utf8();
  }

 private:
  void HandleTextObject(const AXObject& text_object) {
    builder_.Append('<');
    builder_.Append(AXObject::InternalRoleName(text_object.RoleValue()));
    builder_.Append(": ");
    const String name = text_object.ComputedName() + ">\n";
    const AXObject& base_container = *selection_.Base().ContainerObject();
    const AXObject& extent_container = *selection_.Extent().ContainerObject();

    if (base_container == text_object && extent_container == text_object) {
      DCHECK(selection_.Base().IsTextPosition() &&
             selection_.Extent().IsTextPosition());
      const int base_offset = selection_.Base().TextOffset();
      const int extent_offset = selection_.Extent().TextOffset();

      if (base_offset == extent_offset) {
        builder_.Append(name.Left(base_offset));
        builder_.Append('|');
        builder_.Append(name.Substring(base_offset));
        return;
      }

      if (base_offset < extent_offset) {
        builder_.Append(name.Left(base_offset));
        builder_.Append('^');
        builder_.Append(
            name.Substring(base_offset, extent_offset - base_offset));
        builder_.Append('|');
        builder_.Append(name.Substring(extent_offset));
        return;
      }

      builder_.Append(name.Left(extent_offset));
      builder_.Append('|');
      builder_.Append(
          name.Substring(extent_offset, base_offset - extent_offset));
      builder_.Append('^');
      builder_.Append(name.Substring(base_offset));
      return;
    }

    if (base_container == text_object) {
      DCHECK(selection_.Base().IsTextPosition());
      const int base_offset = selection_.Base().TextOffset();

      builder_.Append(name.Left(base_offset));
      builder_.Append('^');
      builder_.Append(name.Substring(base_offset));
      return;
    }

    if (extent_container == text_object) {
      DCHECK(selection_.Extent().IsTextPosition());
      const int extent_offset = selection_.Extent().TextOffset();

      builder_.Append(name.Left(extent_offset));
      builder_.Append('|');
      builder_.Append(name.Substring(extent_offset));
      return;
    }

    builder_.Append(name);
  }

  void HandleObject(const AXObject& object) {
    builder_.Append('<');
    builder_.Append(AXObject::InternalRoleName(object.RoleValue()));

    String name = object.ComputedName();
    if (name.length()) {
      builder_.Append(": ");
      builder_.Append(name);
    }

    builder_.Append(">\n");
    SerializeSubtree(object);
  }

  void HandleSelection(const AXPosition& position) {
    if (!position.IsValid())
      return;

    if (selection_.Extent() == position) {
      builder_.Append('|');
      return;
    }

    if (selection_.Base() != position)
      return;

    builder_.Append('^');
  }

  void SerializeSubtree(const AXObject& subtree) {
    if (subtree.ChildCount() == 0) {
      // Though they are in this particular case both equivalent to an "after
      // object" position, "Before children" and "after children" positions are
      // still valid within empty subtrees.
      const auto position = AXPosition::CreateFirstPositionInObject(subtree);
      HandleSelection(position);
      return;
    }

    for (const AXObject* child : subtree.Children()) {
      DCHECK(child);
      const auto position = AXPosition::CreatePositionBeforeObject(*child);
      HandleSelection(position);
      ++tree_level_;
      builder_.Append(String::FromUTF8(std::string(tree_level_ * 2, '+')));
      if (position.IsTextPosition()) {
        HandleTextObject(*child);
      } else {
        HandleObject(*child);
      }
      --tree_level_;
    }

    // Handle any "after children" positions.
    HandleSelection(AXPosition::CreateLastPositionInObject(subtree));
  }

  StringBuilder builder_;
  int tree_level_;
  AXSelection selection_;
};

// Deserializes an HTML snippet with or without selection markers to an
// |AXSelection| object. A '^' could be present at the selection anchor offset
// and a '|' at the focus offset. If multiple markers are present, the
// deserializer will return multiple |AXSelection| objects. If there are
// multiple markers, the first '|' in DOM order will be matched with the first
// '^' marker, the second '|' with the second '^', and so on. If there are more
// '|'s than '^'s or vice versa, the deserializer will DCHECK. If there are no
// markers, no |AXSelection| objects will be returned. We don't allow '^' and
// '|' markers to appear in anything other than the contents of an HTML node,
// e.g. they are not permitted in aria-labels.
class AXSelectionDeserializer final {
  STACK_ALLOCATED();

 public:
  explicit AXSelectionDeserializer(AXObjectCacheImpl& cache)
      : ax_object_cache_(&cache),
        anchors_(MakeGarbageCollected<VectorOfPairs<Node, int>>()),
        foci_(MakeGarbageCollected<VectorOfPairs<Node, int>>()) {}
  ~AXSelectionDeserializer() = default;

  // Creates an accessibility tree rooted at the given HTML element from the
  // provided HTML snippet and returns |AXSelection| objects that can select the
  // parts of the tree indicated by the selection markers in the snippet.
  const Vector<AXSelection> Deserialize(const std::string& html_snippet,
                                        HTMLElement& element) {
    element.SetInnerHTMLFromString(String::FromUTF8(html_snippet));
    element.GetDocument().View()->UpdateAllLifecyclePhases(
        DocumentLifecycle::LifecycleUpdateReason::kTest);
    AXObject* root = ax_object_cache_->GetOrCreate(&element);
    if (!root || root->IsDetached())
      return {};

    FindSelectionMarkers(*root);
    DCHECK((foci_->size() == 1 && anchors_->size() == 0) ||
           anchors_->size() == foci_->size())
        << "There should be an equal number of '^'s and '|'s in the HTML that "
           "is being deserialized, or if caret placement is required, only a "
           "single '|'.";
    if (foci_->IsEmpty())
      return {};

    Vector<AXSelection> ax_selections;
    if (anchors_->IsEmpty()) {
      // Handle the case when there is just a single '|' marker representing the
      // position of the caret.
      DCHECK(foci_->at(0).first);
      const Position caret(foci_->at(0).first, foci_->at(0).second);
      const auto ax_caret = AXPosition::FromPosition(caret);
      AXSelection::Builder builder;
      ax_selections.push_back(
          builder.SetBase(ax_caret).SetExtent(ax_caret).Build());
      return ax_selections;
    }

    for (size_t i = 0; i < foci_->size(); ++i) {
      DCHECK(anchors_->at(i).first);
      const Position base(*anchors_->at(i).first, anchors_->at(i).second);
      const auto ax_base = AXPosition::FromPosition(base);

      DCHECK(foci_->at(i).first);
      const Position extent(*foci_->at(i).first, foci_->at(i).second);
      const auto ax_extent = AXPosition::FromPosition(extent);
      AXSelection::Builder builder;
      ax_selections.push_back(
          builder.SetBase(ax_base).SetExtent(ax_extent).Build());
    }

    return ax_selections;
  }

 private:
  void HandleCharacterData(const AXObject& text_object) {
    auto* const node = To<CharacterData>(text_object.GetNode());
    Vector<int> base_offsets;
    Vector<int> extent_offsets;
    unsigned number_of_markers = 0;
    StringBuilder builder;
    for (unsigned i = 0; i < node->length(); ++i) {
      const UChar character = node->data()[i];
      if (character == '^') {
        base_offsets.push_back(static_cast<int>(i - number_of_markers));
        ++number_of_markers;
        continue;
      }

      if (character == '|') {
        extent_offsets.push_back(static_cast<int>(i - number_of_markers));
        ++number_of_markers;
        continue;
      }

      builder.Append(character);
    }

    if (base_offsets.IsEmpty() && extent_offsets.IsEmpty())
      return;

    // Remove the markers, otherwise they would be duplicated if the AXSelection
    // is re-serialized.
    node->setData(builder.ToString());
    node->GetDocument().View()->UpdateAllLifecyclePhases(
        DocumentLifecycle::LifecycleUpdateReason::kTest);

    //
    // Non-text selection.
    //

    if (node->ContainsOnlyWhitespaceOrEmpty()) {
      // Since the text object contains only selection markers, this indicates
      // that this is a request for a non-text selection.
      Node* const parent = node->ParentOrShadowHostNode();
      int index_in_parent = static_cast<int>(node->NodeIndex());

      for (size_t i = 0; i < base_offsets.size(); ++i)
        anchors_->emplace_back(parent, index_in_parent);

      for (size_t i = 0; i < extent_offsets.size(); ++i)
        foci_->emplace_back(parent, index_in_parent);

      return;
    }

    //
    // Text selection.
    //

    for (int base_offset : base_offsets)
      anchors_->emplace_back(node, base_offset);
    for (int extent_offset : extent_offsets)
      foci_->emplace_back(node, extent_offset);
  }

  void HandleObject(const AXObject& object) {
    for (const AXObject* child : object.Children()) {
      DCHECK(child);
      FindSelectionMarkers(*child);
    }
  }

  void FindSelectionMarkers(const AXObject& root) {
    const Node* node = root.GetNode();
    if (node && node->IsCharacterDataNode()) {
      HandleCharacterData(root);
      // |root| will need to be detached and replaced with an updated AXObject.
      return;
    }
    HandleObject(root);
  }

  Persistent<AXObjectCacheImpl> const ax_object_cache_;

  // Pairs of anchor nodes + anchor offsets.
  Persistent<VectorOfPairs<Node, int>> anchors_;

  // Pairs of focus nodes + focus offsets.
  Persistent<VectorOfPairs<Node, int>> foci_;
};

}  // namespace

AccessibilitySelectionTest::AccessibilitySelectionTest(
    LocalFrameClient* local_frame_client)
    : AccessibilityTest(local_frame_client) {}

std::string AccessibilitySelectionTest::GetCurrentSelectionText() const {
  const SelectionInDOMTree selection =
      GetFrame().Selection().GetSelectionInDOMTree();
  const auto ax_selection = AXSelection::FromSelection(selection);
  return GetSelectionText(ax_selection);
}

std::string AccessibilitySelectionTest::GetSelectionText(
    const AXSelection& selection) const {
  const AXObject* root = GetAXRootObject();
  if (!root || root->IsDetached())
    return {};
  return AXSelectionSerializer(selection).Serialize(*root);
}

std::string AccessibilitySelectionTest::GetSelectionText(
    const AXSelection& selection,
    const AXObject& subtree) const {
  return AXSelectionSerializer(selection).Serialize(subtree);
}

AXSelection AccessibilitySelectionTest::SetSelectionText(
    const std::string& selection_text) const {
  HTMLElement* body = GetDocument().body();
  if (!body)
    return AXSelection::Builder().Build();
  const Vector<AXSelection> ax_selections =
      AXSelectionDeserializer(GetAXObjectCache())
          .Deserialize(selection_text, *body);
  if (ax_selections.IsEmpty())
    return AXSelection::Builder().Build();
  return ax_selections.front();
}

AXSelection AccessibilitySelectionTest::SetSelectionText(
    const std::string& selection_text,
    HTMLElement& element) const {
  const Vector<AXSelection> ax_selections =
      AXSelectionDeserializer(GetAXObjectCache())
          .Deserialize(selection_text, element);
  if (ax_selections.IsEmpty())
    return AXSelection::Builder().Build();
  return ax_selections.front();
}

void AccessibilitySelectionTest::RunSelectionTest(
    const std::string& test_name) const {
  static const std::string separator_line = '\n' + std::string(80, '=') + '\n';
  const String relative_path = String::FromUTF8(kSelectionTestsRelativePath) +
                               String::FromUTF8(test_name);
  const String test_path = AccessibilityTestDataPath(relative_path);

  const String test_file = test_path + String::FromUTF8(kTestFileSuffix);
  scoped_refptr<SharedBuffer> test_file_buffer = ReadFromFile(test_file);
  auto test_file_chars = test_file_buffer->CopyAs<Vector<char>>();
  std::string test_file_contents;
  std::copy(test_file_chars.begin(), test_file_chars.end(),
            std::back_inserter(test_file_contents));
  ASSERT_FALSE(test_file_contents.empty())
      << "Test file cannot be empty.\n"
      << test_file.Utf8()
      << "\nDid you forget to add a data dependency to the BUILD file?";

  const String ax_file = test_path + String::FromUTF8(kAXTestExpectationSuffix);
  scoped_refptr<SharedBuffer> ax_file_buffer = ReadFromFile(ax_file);
  auto ax_file_chars = ax_file_buffer->CopyAs<Vector<char>>();
  std::string ax_file_contents;
  std::copy(ax_file_chars.begin(), ax_file_chars.end(),
            std::back_inserter(ax_file_contents));
  ASSERT_FALSE(ax_file_contents.empty())
      << "Expectations file cannot be empty.\n"
      << ax_file.Utf8()
      << "\nDid you forget to add a data dependency to the BUILD file?";

  HTMLElement* body = GetDocument().body();
  ASSERT_NE(nullptr, body);
  Vector<AXSelection> ax_selections =
      AXSelectionDeserializer(GetAXObjectCache())
          .Deserialize(test_file_contents, *body);
  std::string actual_ax_file_contents;

  for (auto& ax_selection : ax_selections) {
    ax_selection.Select();
    actual_ax_file_contents += separator_line;
    actual_ax_file_contents += ax_selection.ToString().Utf8();
    actual_ax_file_contents += separator_line;
    actual_ax_file_contents += GetCurrentSelectionText();
  }

  EXPECT_EQ(ax_file_contents, actual_ax_file_contents);
}

}  // namespace test
}  // namespace blink
