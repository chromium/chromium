// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/accessibility/testing/accessibility_selection_test.h"

#include <algorithm>
#include <iterator>
#include <string_view>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_view_util.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/renderer/core/dom/character_data.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object-inl.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/accessibility/ax_position.h"
#include "third_party/blink/renderer/modules/accessibility/ax_selection.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
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
    return StringView(builder_).Utf8();
  }

 private:
  void HandleTextObject(const AXObject& text_object) {
    builder_.Append('<');
    builder_.Append(AXObject::InternalRoleName(text_object.RoleValue()));
    builder_.Append(": ");
    const String name = text_object.ComputedName() + ">\n";
    const AXObject& base_container = *selection_.Anchor().ContainerObject();
    const AXObject& extent_container = *selection_.Focus().ContainerObject();

    if (base_container == text_object && extent_container == text_object) {
      DCHECK(selection_.Anchor().IsTextPosition() &&
             selection_.Focus().IsTextPosition());
      const int base_offset = selection_.Anchor().TextOffset();
      const int extent_offset = selection_.Focus().TextOffset();

      if (base_offset == extent_offset) {
        builder_.Append(name.subview(0, base_offset));
        builder_.Append('|');
        builder_.Append(name.subview(base_offset));
        return;
      }

      if (base_offset < extent_offset) {
        builder_.Append(name.subview(0, base_offset));
        builder_.Append('^');
        builder_.Append(name.subview(base_offset, extent_offset - base_offset));
        builder_.Append('|');
        builder_.Append(name.subview(extent_offset));
        return;
      }

      builder_.Append(name.subview(0, extent_offset));
      builder_.Append('|');
      builder_.Append(name.subview(extent_offset, base_offset - extent_offset));
      builder_.Append('^');
      builder_.Append(name.subview(base_offset));
      return;
    }

    if (base_container == text_object) {
      DCHECK(selection_.Anchor().IsTextPosition());
      const int base_offset = selection_.Anchor().TextOffset();

      builder_.Append(name.subview(0, base_offset));
      builder_.Append('^');
      builder_.Append(name.subview(base_offset));
      return;
    }

    if (extent_container == text_object) {
      DCHECK(selection_.Focus().IsTextPosition());
      const int extent_offset = selection_.Focus().TextOffset();

      builder_.Append(name.subview(0, extent_offset));
      builder_.Append('|');
      builder_.Append(name.subview(extent_offset));
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

    if (selection_.Focus() == position) {
      builder_.Append('|');
      return;
    }

    if (selection_.Anchor() != position) {
      return;
    }

    builder_.Append('^');
  }

  void SerializeSubtree(const AXObject& subtree) {
    if (!subtree.ChildCountIncludingIgnored()) {
      // Though they are in this particular case both equivalent to an "after
      // object" position, "Before children" and "after children" positions are
      // still valid within empty subtrees.
      const auto position = AXPosition::CreateFirstPositionInObject(subtree);
      HandleSelection(position);
      return;
    }

    for (const AXObject* child : subtree.ChildrenIncludingIgnored()) {
      DCHECK(child);
      const auto position = AXPosition::CreatePositionBeforeObject(*child);
      HandleSelection(position);
      ++tree_level_;
      builder_.Append(String::FromUtf8(std::string(tree_level_ * 2, '+')));
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
// '|' markers to appear in anything other than the contents of an HTML node or
// text controls like <textarea> and some <input> types, e.g. they are not
// permitted in aria-labels.
class AXSelectionDeserializer final {
  STACK_ALLOCATED();

 public:
  explicit AXSelectionDeserializer(AXObjectCacheImpl& cache)
      : ax_object_cache_(&cache),
        anchors_(MakeGarbageCollected<Holder>()),
        foci_(MakeGarbageCollected<Holder>()) {}
  ~AXSelectionDeserializer() = default;

  const AXObjectCacheImpl& GetAXObjectCache() const {
    return *ax_object_cache_;
  }

  // Creates an accessibility tree rooted at the given HTML element from the
  // provided HTML snippet and returns |AXSelection| objects that can select the
  // parts of the tree indicated by the selection markers in the snippet.
  const Vector<AXSelection> Deserialize(const std::string_view& html_snippet,
                                        HTMLElement& element) {
    element.SetInnerHTMLWithoutTrustedTypes(String::FromUtf8(html_snippet));
    element.GetDocument().View()->UpdateAllLifecyclePhasesForTest();
    AXObject* root = ax_object_cache_->Get(&element);
    if (!root || root->IsDetached())
      return {};

    FindSelectionMarkers(*root);
    DCHECK((foci()->size() == 1 && anchors()->size() == 0) ||
           anchors()->size() == foci()->size())
        << "There should be an equal number of '^'s and '|'s in the HTML that "
           "is being deserialized, or if caret placement is required, only a "
           "single '|'.";
    if (foci()->empty()) {
      return {};
    }

    Vector<AXSelection> ax_selections;
    if (anchors()->empty()) {
      // Handle the case when there is just a single '|' marker representing the
      // position of the caret.
      DCHECK(foci()->at(0).first);
      const Position caret(foci()->at(0).first, foci()->at(0).second);
      const auto ax_caret = AXPosition::FromPosition(caret, GetAXObjectCache());
      AXSelection::Builder builder(GetAXObjectCache());
      ax_selections.push_back(
          builder.SetAnchor(ax_caret).SetFocus(ax_caret).Build());
      return ax_selections;
    }

    for (wtf_size_t i = 0; i < foci()->size(); ++i) {
      DCHECK(anchors()->at(i).first);
      const Position anchor(*anchors()->at(i).first, anchors()->at(i).second);
      const auto ax_anchor =
          AXPosition::FromPosition(anchor, GetAXObjectCache());

      DCHECK(foci()->at(i).first);
      const Position focus(*foci()->at(i).first, foci()->at(i).second);
      const auto ax_focus = AXPosition::FromPosition(focus, GetAXObjectCache());
      AXSelection::Builder builder(GetAXObjectCache());
      ax_selections.push_back(
          builder.SetAnchor(ax_anchor).SetFocus(ax_focus).Build());
    }

    return ax_selections;
  }

 private:
  // Extracts the '^' and '|' selection marker offsets from the provided text,
  // and inserts them into anchor_offsets/focus_offsets. Returns a string
  // containing the |text| without the the selection markers.
  String ExtractSelectionMarkers(const String& text,
                                 Vector<int>& anchor_offsets,
                                 Vector<int>& focus_offsets) {
    StringBuilder builder;
    unsigned number_of_markers = 0;
    for (unsigned i = 0; i < text.length(); ++i) {
      const UChar character = text[i];
      if (character == '^') {
        anchor_offsets.push_back(static_cast<int>(i - number_of_markers));
        ++number_of_markers;
        continue;
      }

      if (character == '|') {
        focus_offsets.push_back(static_cast<int>(i - number_of_markers));
        ++number_of_markers;
        continue;
      }

      builder.Append(character);
    }

    return builder.ToString();
  }

  // Extracts selection markers offsets from text control elements.
  // The default value of the text control is updated to not include the
  // selection markers.
  void HandleTextControlElement(const AXObject& text_control_object) {
    auto* const field = To<TextControlElement>(text_control_object.GetNode());
    Vector<int> anchor_offsets;
    Vector<int> focus_offsets;
    const String extracted_text =
        ExtractSelectionMarkers(field->Value(), anchor_offsets, focus_offsets);

    if (anchor_offsets.empty() && focus_offsets.empty()) {
      return;
    }

    // Remove the markers from the HTML of the text control, instead of just
    // updating the current value of the control.
    if (auto* input = DynamicTo<HTMLInputElement>(field)) {
      input->setAttribute(html_names::kValueAttr, AtomicString(extracted_text));
    } else if (auto* textarea = DynamicTo<HTMLTextAreaElement>(field)) {
      textarea->setTextContent(extracted_text);
    }

    field->GetDocument().View()->UpdateAllLifecyclePhasesForTest();

    for (int anchor_offset : anchor_offsets) {
      const Position anchor =
          field->VisiblePositionForIndex(anchor_offset).DeepEquivalent();
      anchors()->emplace_back(anchor.AnchorNode(),
                              anchor.OffsetInContainerNode());
    }

    for (int focus_offset : focus_offsets) {
      const Position focus =
          field->VisiblePositionForIndex(focus_offset).DeepEquivalent();
      foci()->emplace_back(focus.AnchorNode(), focus.OffsetInContainerNode());
    }
  }

  // Extracts selection markers offsets from a character data node.
  // The text of the node is updated to not include the selection markers.
  void HandleCharacterData(const AXObject& text_object) {
    auto* const node = To<CharacterData>(text_object.GetNode());
    Vector<int> anchor_offsets;
    Vector<int> focus_offsets;
    const String extracted_text =
        ExtractSelectionMarkers(node->data(), anchor_offsets, focus_offsets);

    if (anchor_offsets.empty() && focus_offsets.empty()) {
      return;
    }

    // Remove the markers, otherwise they would be duplicated if the AXSelection
    // is re-serialized.
    node->setData(extracted_text);
    node->GetDocument().View()->UpdateAllLifecyclePhasesForTest();

    //
    // Non-text selection.
    //

    if (node->ContainsOnlyWhitespaceOrEmpty()) {
      // Since the text object contains only selection markers, this indicates
      // that this is a request for a non-text selection.
      Node* const parent = node->ParentOrShadowHostNode();
      int index_in_parent = static_cast<int>(node->NodeIndex());

      for (size_t i = 0; i < anchor_offsets.size(); ++i) {
        anchors()->emplace_back(parent, index_in_parent);
      }

      for (size_t i = 0; i < focus_offsets.size(); ++i) {
        foci()->emplace_back(parent, index_in_parent);
      }

      return;
    }

    //
    // Text selection.
    //

    for (int anchor_offset : anchor_offsets) {
      anchors()->emplace_back(node, anchor_offset);
    }
    for (int focus_offset : focus_offsets) {
      foci()->emplace_back(node, focus_offset);
    }
  }

  void HandleObject(const AXObject& object) {
    // Make a copy of the children, because they may be cleared when a sibling
    // is invalidated and calls SetNeedsToUpdateChildren() on the parent.
    const auto children = object.ChildrenIncludingIgnored();

    for (const AXObject* child : children) {
      DCHECK(child);
      FindSelectionMarkers(*child);
    }
  }

  void FindSelectionMarkers(const AXObject& root) {
    const Node* node = root.GetNode();
    if (node && IsTextControl(node)) {
      HandleTextControlElement(root);
      return;
    }
    if (node && node->IsCharacterDataNode()) {
      HandleCharacterData(root);
      // |root| will need to be detached and replaced with an updated AXObject.
      return;
    }
    HandleObject(root);
  }

  Persistent<AXObjectCacheImpl> const ax_object_cache_;

  using Holder = DisallowNewWrapper<VectorOfPairs<Node, int>>;

  VectorOfPairs<Node, int>* anchors() const { return &anchors_->Value(); }

  VectorOfPairs<Node, int>* foci() const { return &foci_->Value(); }

  // Pairs of anchor nodes + anchor offsets.
  Persistent<Holder> anchors_;
  // Pairs of focus nodes + focus offsets.
  Persistent<Holder> foci_;
};

}  // namespace

AccessibilitySelectionTest::AccessibilitySelectionTest(
    LocalFrameClient* local_frame_client)
    : AccessibilityTest(local_frame_client) {}

void AccessibilitySelectionTest::SetUp() {
  RenderingTest::SetUp();
  // Do not include noisy inline textboxes in selection tests.
  ax_context_ =
      std::make_unique<AXContext>(GetDocument(), ui::AXMode::kWebContents);
}

std::string AccessibilitySelectionTest::GetCurrentSelectionText() const {
  const SelectionInDOMTree selection =
      GetFrame().Selection().GetSelectionInDOMTree();
  const auto ax_selection =
      AXSelection::FromSelection(selection, GetAXObjectCache());
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
    return AXSelection::Builder(GetAXObjectCache()).Build();

  return SetSelectionText(selection_text, *body);
}

AXSelection AccessibilitySelectionTest::SetSelectionText(
    const std::string& selection_text,
    HTMLElement& element) const {
  const Vector<AXSelection> ax_selections =
      SetMultipleSelectionText(selection_text, element);
  if (ax_selections.empty())
    return AXSelection::Builder(GetAXObjectCache()).Build();
  return ax_selections.front();
}

Vector<AXSelection> AccessibilitySelectionTest::SetMultipleSelectionText(
    const std::string& selection_text) const {
  HTMLElement* body = GetDocument().body();
  if (!body) {
    return Vector<AXSelection>();
  }

  return SetMultipleSelectionText(selection_text, *body);
}

Vector<AXSelection> AccessibilitySelectionTest::SetMultipleSelectionText(
    const std::string& selection_text,
    HTMLElement& element) const {
  return AXSelectionDeserializer(GetAXObjectCache())
      .Deserialize(selection_text, element);
}

void AccessibilitySelectionTest::RunSelectionTest(
    const std::string& test_name,
    const std::string& suffix) const {
  static const std::string separator_line = '\n' + std::string(80, '=') + '\n';
  const String relative_path = String::FromUtf8(kSelectionTestsRelativePath) +
                               String::FromUtf8(test_name);
  const String test_path = test::AccessibilityTestDataPath(relative_path);

  const String test_file = test_path + String::FromUtf8(kTestFileSuffix);
  std::optional<Vector<char>> test_file_data = test::ReadFromFile(test_file);
  ASSERT_TRUE(test_file_data)
      << "Test file cannot be empty.\n"
      << test_file.Utf8()
      << "\nDid you forget to add a data dependency to the BUILD file?";

  const String ax_file =
      test_path +
      String::FromUtf8(suffix.empty() ? kAXTestExpectationSuffix : suffix);
  std::optional<Vector<char>> ax_file_data = test::ReadFromFile(ax_file);
  ASSERT_TRUE(ax_file_data)
      << "Expectations file cannot be empty.\n"
      << ax_file.Utf8()
      << "\nDid you forget to add a data dependency to the BUILD file?";
  std::string_view ax_file_contents = base::as_string_view(*ax_file_data);

  HTMLElement* body = GetDocument().body();
  ASSERT_NE(nullptr, body);
  Vector<AXSelection> ax_selections =
      AXSelectionDeserializer(GetAXObjectCache())
          .Deserialize(base::as_string_view(*test_file_data), *body);
  std::string actual_ax_file_contents;

  for (auto& ax_selection : ax_selections) {
    ax_selection.Select();
    actual_ax_file_contents += separator_line;
    actual_ax_file_contents += ax_selection.ToString().Utf8();
    actual_ax_file_contents += separator_line;
    actual_ax_file_contents += GetCurrentSelectionText();
  }

  EXPECT_TRUE(ax_file_contents == actual_ax_file_contents)
      << "\nSelection does not match expectations. Legend: ^=selection start  "
         "|=selection end"
      << "\n\nExpected:\n--------\n"
      << ax_file_contents << "\n\nActual:\n------\n"
      << actual_ax_file_contents;

  // Uncomment these lines to write the output to the expectations file.
  // TODO(dmazzoni): make this a command-line parameter.
  // if (ax_file_contents != actual_ax_file_contents)
  //  base::WriteFile(WebStringToFilePath(ax_file), actual_ax_file_contents);
}

}  // namespace blink
