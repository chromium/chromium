// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/character_range_mapper.h"

#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"

namespace blink {

class CharacterRangeMapperTest : public EditingTestBase {
 protected:
  template <typename Strategy>
  CharacterRange CreateCharacterRange(
      const EphemeralRangeTemplate<Strategy>& scope,
      const EphemeralRangeTemplate<Strategy>& range,
      const TextIteratorBehavior& behavior =
          TextIteratorBehavior::DefaultRangeLengthBehavior()) {
    return CharacterRangeMapperAlgorithm<Strategy>::CreateCharacterRange(
        scope, range, behavior);
  }

  template <typename Strategy>
  EphemeralRangeTemplate<Strategy> ResolveCharacterRange(
      const EphemeralRangeTemplate<Strategy>& scope,
      const CharacterRange& range,
      const TextIteratorBehavior& behavior =
          TextIteratorBehavior::DefaultRangeLengthBehavior()) {
    return CharacterRangeMapperAlgorithm<Strategy>::ResolveCharacterRange(
        scope, range, behavior);
  }
};

TEST_F(CharacterRangeMapperTest, RangeAtBeginningOfScope) {
  const EphemeralRange selection_range =
      SetSelectionTextToBody("<div>^Hello|, World!</div>").ComputeRange();
  Node* div = QuerySelector("div");
  const EphemeralRange scope = EphemeralRange::RangeOfContents(*div);

  const CharacterRange character_range =
      CreateCharacterRange(scope, selection_range);
  EXPECT_EQ(character_range, (CharacterRange{0, 5}));

  const EphemeralRange resolved_range =
      ResolveCharacterRange(scope, character_range);
  EXPECT_EQ(selection_range, resolved_range);
}

TEST_F(CharacterRangeMapperTest, RangeAtEndOfScope) {
  const EphemeralRange selection_range =
      SetSelectionTextToBody("<div>Hello, ^World!|</div>").ComputeRange();
  Node* div = QuerySelector("div");
  const EphemeralRange scope = EphemeralRange::RangeOfContents(*div);

  const CharacterRange character_range =
      CreateCharacterRange(scope, selection_range);
  EXPECT_EQ(character_range, (CharacterRange{7, 6}));

  const EphemeralRange resolved_range =
      ResolveCharacterRange(scope, character_range);
  EXPECT_EQ(selection_range, resolved_range);
}

TEST_F(CharacterRangeMapperTest, RangeSameAsScope) {
  const EphemeralRange selection_range =
      SetSelectionTextToBody("<div>Hello, ^World|!</div>").ComputeRange();
  const EphemeralRange scope = selection_range;

  const CharacterRange character_range =
      CreateCharacterRange(scope, selection_range);
  EXPECT_EQ(character_range, (CharacterRange{0, 5}));

  const EphemeralRange resolved_range =
      ResolveCharacterRange(scope, character_range);
  EXPECT_EQ(selection_range, resolved_range);
}

TEST_F(CharacterRangeMapperTest, RangeMatchesResolvedAffinity) {
  // The resolved range should anchor inside the <span>, matching the
  // selection's anchor and focus positions.
  const EphemeralRange selection_range =
      SetSelectionTextToBody("<div>Hello, <span>^World|</span>!</div>")
          .ComputeRange();
  Node* div = QuerySelector("div");
  const EphemeralRange scope = EphemeralRange::RangeOfContents(*div);

  const CharacterRange character_range =
      CreateCharacterRange(scope, selection_range);
  EXPECT_EQ(character_range, (CharacterRange{7, 5}));

  const EphemeralRange resolved_range =
      ResolveCharacterRange(scope, character_range);
  Text* text = To<Text>(QuerySelector("span")->firstChild());
  const EphemeralRange expected_range(Position(text, 0), Position(text, 5));
  EXPECT_EQ(expected_range, resolved_range);
  EXPECT_EQ(selection_range, resolved_range);
}

TEST_F(CharacterRangeMapperTest, RangeDoesNotMatchResolvedAffinity) {
  // The resolved range should anchor inside the <span>, therefore it does not
  // match the selection's anchor and focus positions.
  const EphemeralRange selection_range =
      SetSelectionTextToBody("<div>Hello, ^<span>World</span>|!</div>")
          .ComputeRange();
  Node* div = QuerySelector("div");
  const EphemeralRange scope = EphemeralRange::RangeOfContents(*div);

  const CharacterRange character_range =
      CreateCharacterRange(scope, selection_range);
  EXPECT_EQ(character_range, (CharacterRange{7, 5}));

  const EphemeralRange resolved_range =
      ResolveCharacterRange(scope, character_range);
  Text* text = To<Text>(QuerySelector("span")->firstChild());
  const EphemeralRange expected_range(Position(text, 0), Position(text, 5));
  EXPECT_EQ(expected_range, resolved_range);
  EXPECT_NE(selection_range, resolved_range);
}

TEST_F(CharacterRangeMapperTest, RangeAcrossInlineElements) {
  const EphemeralRange selection_range =
      SetSelectionTextToBody("<div><b>^Hello</b>, <i>World|</i>!</div>")
          .ComputeRange();
  Node* div = QuerySelector("div");
  const EphemeralRange scope = EphemeralRange::RangeOfContents(*div);

  const CharacterRange character_range =
      CreateCharacterRange(scope, selection_range);
  EXPECT_EQ(character_range, (CharacterRange{0, 12}));

  const EphemeralRange resolved_range =
      ResolveCharacterRange(scope, character_range);
  EXPECT_EQ(resolved_range, selection_range);
}

TEST_F(CharacterRangeMapperTest, RangeAcrossParagraphs) {
  const EphemeralRange selection_range =
      SetSelectionTextToBody("<div><p>^One</p><p>Two|</p></div>")
          .ComputeRange();
  Node* div = QuerySelector("div");
  const EphemeralRange scope = EphemeralRange::RangeOfContents(*div);

  const CharacterRange character_range =
      CreateCharacterRange(scope, selection_range);
  // Expected characters: "One\n\nTwo".
  EXPECT_EQ(character_range, (CharacterRange{0, 8}));

  const EphemeralRange resolved_range =
      ResolveCharacterRange(scope, character_range);
  EXPECT_EQ(resolved_range, selection_range);
}

TEST_F(CharacterRangeMapperTest, EmptyRange) {
  const EphemeralRange selection_range =
      SetSelectionTextToBody("<div>Hello^|, World!</div>").ComputeRange();
  Node* div = QuerySelector("div");
  const EphemeralRange scope = EphemeralRange::RangeOfContents(*div);

  const CharacterRange character_range =
      CreateCharacterRange(scope, selection_range);
  EXPECT_EQ(character_range, (CharacterRange{5, 0}));

  const EphemeralRange resolved_range =
      ResolveCharacterRange(scope, character_range);
  EXPECT_EQ(selection_range, resolved_range);
}

TEST_F(CharacterRangeMapperTest, EmptyRangeAtBeginning) {
  // While normally empty character ranges have upstream affinity, they are
  // never resolved to a position outside the scope range i.e., they effectively
  // have downstream affinity for the first position. Here, the resolved
  // position should anchor inside the <div> rather than the element before it.
  const EphemeralRange selection_range =
      SetSelectionTextToBody("<div>^|Hello, World!</div>").ComputeRange();
  Node* div = QuerySelector("div");
  const EphemeralRange scope = EphemeralRange::RangeOfContents(*div);

  const CharacterRange character_range =
      CreateCharacterRange(scope, selection_range);
  EXPECT_EQ(character_range, (CharacterRange{0, 0}));

  const EphemeralRange resolved_range =
      ResolveCharacterRange(scope, character_range);
  EXPECT_EQ(selection_range, resolved_range);
}

TEST_F(CharacterRangeMapperTest, EmptyRangeAtEnd) {
  const EphemeralRange selection_range =
      SetSelectionTextToBody("<div>Hello, World!^|</div>").ComputeRange();
  Node* div = QuerySelector("div");
  const EphemeralRange scope = EphemeralRange::RangeOfContents(*div);

  const CharacterRange character_range =
      CreateCharacterRange(scope, selection_range);
  EXPECT_EQ(character_range, (CharacterRange{13, 0}));

  const EphemeralRange resolved_range =
      ResolveCharacterRange(scope, character_range);
  EXPECT_EQ(selection_range, resolved_range);
}

TEST_F(CharacterRangeMapperTest, EmptyRangeMatchesResolvedAffinityAtEnd) {
  // The resolved range should anchor inside the <span> as empty character
  // ranges have upstream affinity.
  const EphemeralRange selection_range =
      SetSelectionTextToBody("<div>Hello, <span>World^|</span>!</div>")
          .ComputeRange();
  Node* div = QuerySelector("div");
  const EphemeralRange scope = EphemeralRange::RangeOfContents(*div);

  const CharacterRange character_range =
      CreateCharacterRange(scope, selection_range);
  EXPECT_EQ(character_range, (CharacterRange{12, 0}));

  const EphemeralRange resolved_range =
      ResolveCharacterRange(scope, character_range);
  Text* text = To<Text>(QuerySelector("span")->firstChild());
  const EphemeralRange expected_range(Position(text, 5), Position(text, 5));
  EXPECT_EQ(expected_range, resolved_range);
  EXPECT_EQ(selection_range, resolved_range);
}

TEST_F(CharacterRangeMapperTest,
       EmptyRangeDoesNotMatchResolvedAffinityAtBeginning) {
  // The resolved range should anchor inside the <span> as empty character
  // ranges have upstream affinity.
  const EphemeralRange selection_range =
      SetSelectionTextToBody("<div>Hello, <span>World</span>^|!</div>")
          .ComputeRange();
  Node* div = QuerySelector("div");
  const EphemeralRange scope = EphemeralRange::RangeOfContents(*div);

  const CharacterRange character_range =
      CreateCharacterRange(scope, selection_range);
  EXPECT_EQ(character_range, (CharacterRange{12, 0}));

  const EphemeralRange resolved_range =
      ResolveCharacterRange(scope, character_range);
  Text* text = To<Text>(QuerySelector("span")->firstChild());
  const EphemeralRange expected_range(Position(text, 5), Position(text, 5));
  EXPECT_EQ(expected_range, resolved_range);
  EXPECT_NE(selection_range, resolved_range);
}

TEST_F(CharacterRangeMapperTest, RangeInTextArea) {
  SetBodyContent("<textarea>Hello, World!</textarea>");
  auto* textarea = To<TextControlElement>(QuerySelector("textarea"));
  textarea->SetSelectionRange(7, 12);
  const EphemeralRange selection_range = textarea->Selection().ComputeRange();

  // Must use the |InnerEditorElement| or a descendant as the scope
  // for TextControlElements.
  const EphemeralRange scope =
      EphemeralRange::RangeOfContents(*textarea->InnerEditorElement());

  const CharacterRange character_range =
      CreateCharacterRange(scope, selection_range);
  EXPECT_EQ(character_range, (CharacterRange{7, 5}));

  const EphemeralRange resolved_range =
      ResolveCharacterRange(scope, character_range);
  EXPECT_EQ(selection_range, resolved_range);
}

TEST_F(CharacterRangeMapperTest, RangeInInputField) {
  SetBodyContent("<input value='Hello, World!'>");
  auto* input = To<TextControlElement>(QuerySelector("input"));
  input->SetSelectionRange(0, 5);
  const EphemeralRange selection_range = input->Selection().ComputeRange();

  // Must use the |InnerEditorElement| or a descendant as the scope
  // for TextControlElements.
  const EphemeralRange scope =
      EphemeralRange::RangeOfContents(*input->InnerEditorElement());

  const CharacterRange character_range =
      CreateCharacterRange(scope, selection_range);
  EXPECT_EQ(character_range, (CharacterRange{0, 5}));

  const EphemeralRange resolved_range =
      ResolveCharacterRange(scope, character_range);
  EXPECT_EQ(selection_range, resolved_range);
}

TEST_F(CharacterRangeMapperTest, RangeInShadowDOM) {
  const SelectionInDOMTree selection = SetSelectionTextToBody(
      R"HTML(
      <div id="host">
        <template data-mode="open">
          <slot name="slot1"></slot>
          <slot name="slot2"></slot>
        </template>
        <div id="shadow1" slot="slot2">1^23|4</div>
        <div id="shadow2" slot="slot1">abc</div>
      </div>
      )HTML");
  const EphemeralRange selection_range_in_dom_tree = selection.ComputeRange();
  const EphemeralRangeInFlatTree selection_range_in_flat_tree =
      ConvertToSelectionInFlatTree(selection).ComputeRange();

  Node* shadow_element = GetDocument().getElementById(AtomicString("shadow1"));
  const EphemeralRange scope_in_dom_tree =
      EphemeralRange::RangeOfContents(*shadow_element);
  const EphemeralRangeInFlatTree scope_in_flat_tree =
      EphemeralRangeInFlatTree::RangeOfContents(*shadow_element);

  const CharacterRange character_range_in_dom_tree =
      CreateCharacterRange(scope_in_dom_tree, selection_range_in_dom_tree);
  EXPECT_EQ(character_range_in_dom_tree, (CharacterRange{1, 2}));

  const EphemeralRange resolved_range_in_dom_tree =
      ResolveCharacterRange(scope_in_dom_tree, character_range_in_dom_tree);
  EXPECT_EQ(selection_range_in_dom_tree, resolved_range_in_dom_tree);

  const CharacterRange character_range_in_flat_tree =
      CreateCharacterRange(scope_in_flat_tree, selection_range_in_flat_tree);
  EXPECT_EQ(character_range_in_flat_tree, (CharacterRange{1, 2}));

  const EphemeralRangeInFlatTree resolved_range_in_flat_tree =
      ResolveCharacterRange(scope_in_flat_tree, character_range_in_flat_tree);
  EXPECT_EQ(selection_range_in_flat_tree, resolved_range_in_flat_tree);
}

TEST_F(CharacterRangeMapperTest, RangeInShadowDOMAndScopeInLightDOM) {
  const SelectionInDOMTree selection = SetSelectionTextToBody(
      R"HTML(
      <div id="host">
        <template data-mode="open">
          <slot name="slot1"></slot>
          <slot name="slot2"></slot>
        </template>
        <div id="shadow1" slot="slot2">1234</div>
        <div id="shadow2" slot="slot1">^abc|</div>
      </div>
      )HTML");
  const EphemeralRange selection_range_in_dom_tree = selection.ComputeRange();
  const EphemeralRangeInFlatTree selection_range_in_flat_tree =
      ConvertToSelectionInFlatTree(selection).ComputeRange();

  Node* host = GetDocument().getElementById(AtomicString("host"));
  const EphemeralRange scope_in_dom_tree =
      EphemeralRange::RangeOfContents(*host);
  const EphemeralRangeInFlatTree scope_in_flat_tree =
      EphemeralRangeInFlatTree::RangeOfContents(*host);

  // Expected characters: "1234\nabc".
  const CharacterRange character_range_in_dom_tree =
      CreateCharacterRange(scope_in_dom_tree, selection_range_in_dom_tree);
  EXPECT_EQ(character_range_in_dom_tree, (CharacterRange{5, 3}));

  const EphemeralRange resolved_range_in_dom_tree =
      ResolveCharacterRange(scope_in_dom_tree, character_range_in_dom_tree);
  EXPECT_EQ(selection_range_in_dom_tree, resolved_range_in_dom_tree);

  // Expected characters: "abc\n1234".
  const CharacterRange character_range_in_flat_tree =
      CreateCharacterRange(scope_in_flat_tree, selection_range_in_flat_tree);
  EXPECT_EQ(character_range_in_flat_tree, (CharacterRange{0, 3}));

  const EphemeralRangeInFlatTree resolved_range_in_flat_tree =
      ResolveCharacterRange(scope_in_flat_tree, character_range_in_flat_tree);
  EXPECT_EQ(selection_range_in_flat_tree, resolved_range_in_flat_tree);
}

}  // namespace blink
