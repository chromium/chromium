// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/plain_text_range.h"

#include <string>

#include "base/strings/strcat.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

namespace {

// U+20B9F, which is a surrogate pair in UTF-16.
constexpr char kSurrogatePair[] = "\xF0\xA0\xAE\x9F";

}  // namespace

class PlainTextRangeTest : public EditingTestBase {
 protected:
  // Checks that CreateRange*() functions create the same ranges with and
  // without FastPlainTextRangeCreateRange for every range of plain text
  // offsets in `scope`, including ranges that start or end past its text. If
  // `enters_open_shadow_roots` is true, checks CreateRangeForSelection(), whose
  // text iterator enters open shadow roots. Otherwise, checks CreateRange() and
  // CreateRangeForSelectionIndexing(), whose text iterators don't.
  void ExpectSameRangesAsLegacy(const ContainerNode& scope,
                                bool enters_open_shadow_roots) {
    // Emitting characters between all visible positions makes this text at
    // least as long as the text that each checked function iterates. The +2
    // covers offsets past the end.
    const wtf_size_t max_offset =
        PlainText(EphemeralRange::RangeOfContents(scope),
                  TextIteratorBehavior::Builder()
                      .SetEmitsObjectReplacementCharacter(true)
                      .SetEmitsCharactersBetweenAllVisiblePositions(true)
                      .SetEntersOpenShadowRoots(enters_open_shadow_roots)
                      .Build())
            .length() +
        2;
    if (enters_open_shadow_roots) {
      ExpectFunctionMatchesLegacy(scope, max_offset, "CreateRangeForSelection",
                                  &PlainTextRange::CreateRangeForSelection);
      return;
    }
    ExpectFunctionMatchesLegacy(scope, max_offset, "CreateRange",
                                &PlainTextRange::CreateRange);
    ExpectFunctionMatchesLegacy(
        scope, max_offset, "CreateRangeForSelectionIndexing",
        &PlainTextRange::CreateRangeForSelectionIndexing);
  }

  void ExpectSameRangesAsLegacyInContentEditable(const std::string& html) {
    SCOPED_TRACE(html);
    SetBodyContent(
        base::StrCat({"<div contenteditable id='sample'>", html, "</div>"}));
    const Element& sample = *GetElementById("sample");
    ExpectSameRangesAsLegacy(sample, /*enters_open_shadow_roots=*/true);
    ExpectSameRangesAsLegacy(sample, /*enters_open_shadow_roots=*/false);
  }

  // `html` has a text control with the id "sample".
  void ExpectSameRangesAsLegacyInTextControl(const std::string& html) {
    SCOPED_TRACE(html);
    SetBodyContent(html);
    const ContainerNode& inner_editor =
        *To<TextControlElement>(GetElementById("sample"))->InnerEditorElement();
    ExpectSameRangesAsLegacy(inner_editor, /*enters_open_shadow_roots=*/true);
    ExpectSameRangesAsLegacy(inner_editor, /*enters_open_shadow_roots=*/false);
  }

  // Sets up a contenteditable with the id "sample" that contains a shadow host
  // with the id "host". The host's `mode` shadow root has text and line breaks
  // around a slot for the host's text. Returns the shadow root.
  ShadowRoot& SetContentEditableWithShadowHost(ShadowRootMode mode) {
    SetBodyContent(
        "<div contenteditable id='sample'>ab<span id='host'>light</span>cd"
        "</div>");
    ShadowRoot& shadow_root =
        GetElementById("host")->AttachShadowRootForTesting(mode);
    shadow_root.SetInnerHTMLWithoutTrustedTypes("wx<br><slot></slot><br>yz");
    UpdateAllLifecyclePhasesForTest();
    return shadow_root;
  }

 private:
  using CreateRangeFunction =
      EphemeralRange (PlainTextRange::*)(const ContainerNode&) const;

  // Checks that `create_range`, the CreateRange*() function called `name`,
  // creates the same ranges with and without FastPlainTextRangeCreateRange for
  // every range of plain text offsets in `scope` up to `max_offset`.
  void ExpectFunctionMatchesLegacy(const ContainerNode& scope,
                                   wtf_size_t max_offset,
                                   const char* name,
                                   CreateRangeFunction create_range) {
    SCOPED_TRACE(name);
    // Track that the function created at least one range, so the EXPECT_EQ()s
    // below can't all pass just because neither computation created any ranges.
    // This can't assert a range at a specific offset because a scope may have
    // no text (e.g. an empty or entirely non-displayed editable).
    bool created_range = false;
    for (wtf_size_t start = 0; start <= max_offset; ++start) {
      for (wtf_size_t end = start; end <= max_offset; ++end) {
        SCOPED_TRACE(testing::Message() << "start=" << start << " end=" << end);
        const PlainTextRange plain_text_range(start, end);
        ScopedFastPlainTextRangeCreateRangeForTest fast(true);
        const EphemeralRange range = (plain_text_range.*create_range)(scope);
        created_range |= range.IsNotNull();

        ScopedFastPlainTextRangeCreateRangeForTest legacy(false);
        EXPECT_EQ((plain_text_range.*create_range)(scope), range);
      }
    }
    EXPECT_TRUE(created_range);
  }

  // Turns FastPlainTextRangeCreateRange on for every test, so that no test
  // depends on the flag's default. ExpectFunctionMatchesLegacy() turns it off
  // to create the legacy ranges.
  ScopedFastPlainTextRangeCreateRangeForTest scoped_feature_{true};
};

TEST_F(PlainTextRangeTest, RangeContainingTableCellBoundary) {
  SetBodyInnerHTML(
      "<table id='sample' contenteditable><tr><td>a</td><td "
      "id='td2'>b</td></tr></table>");
  Element* table = GetElementById("sample");

  PlainTextRange plain_text_range(2, 2);
  const EphemeralRange& range = plain_text_range.CreateRange(*table);
  EXPECT_EQ(
      "<table contenteditable id=\"sample\"><tbody><tr><td>a</td><td "
      "id=\"td2\">|b</td></tr></tbody></table>",
      GetCaretTextFromBody(range.StartPosition()));
  EXPECT_TRUE(range.IsCollapsed());
}

// A <textarea> represents each line break with a <br>, so the runs before a
// range include newlines emitted for <br>s.
TEST_F(PlainTextRangeTest, CreateRangeInTextAreaMatchesLegacy) {
  ExpectSameRangesAsLegacyInTextControl(
      "<textarea id='sample'>ab\n\ncd\nef  gh\n\n</textarea>");
  ExpectSameRangesAsLegacyInTextControl(
      base::StrCat({"<textarea id='sample'>", kSurrogatePair, "a",
                    kSurrogatePair, "\nb", kSurrogatePair, "</textarea>"}));
}

TEST_F(PlainTextRangeTest, CreateRangeInInputMatchesLegacy) {
  ExpectSameRangesAsLegacyInTextControl(
      "<input id='sample' value='hello   world'>");
  ExpectSameRangesAsLegacyInTextControl(
      base::StrCat({"<input id='sample' value='a", kSurrogatePair, "b'>"}));
}

// An empty text control's inner editor has no text.
TEST_F(PlainTextRangeTest, CreateRangeInEmptyTextControlMatchesLegacy) {
  ExpectSameRangesAsLegacyInTextControl("<input id='sample'>");
  ExpectSameRangesAsLegacyInTextControl("<textarea id='sample'></textarea>");
}

TEST_F(PlainTextRangeTest, CreateRangeInContentEditableMatchesLegacy) {
  // Line breaks, including at the start and the end.
  ExpectSameRangesAsLegacyInContentEditable("<br>ab<br><br>cd<br>");
  // Blocks, whose separators are emitted only after text.
  ExpectSameRangesAsLegacyInContentEditable("<p>first</p><p>second</p>");
  ExpectSameRangesAsLegacyInContentEditable(
      "<div>first</div><div>second</div>");
  // A replaced element, an inline element and a table.
  ExpectSameRangesAsLegacyInContentEditable(
      "ab<img width='10' height='10'>cd<b>ef</b>"
      "<table><tr><td>g</td><td>h</td></tr></table>ij");
}

// CreateRangeForSelection() enters open shadow roots, so for it the runs before
// a range can include a shadow host's shadow text and line breaks as well as
// its slotted text.
TEST_F(PlainTextRangeTest, CreateRangeEnteringOpenShadowRootMatchesLegacy) {
  const ShadowRoot& shadow_root =
      SetContentEditableWithShadowHost(ShadowRootMode::kOpen);
  const Element& sample = *GetElementById("sample");

  // Check that CreateRangeForSelection() enters the shadow root: the offsets
  // [2, 4) are "wx" in the shadow tree.
  Node* const shadow_text = shadow_root.firstChild();
  EXPECT_EQ(EphemeralRange(Position(shadow_text, 0), Position(shadow_text, 2)),
            PlainTextRange(2, 4).CreateRangeForSelection(sample));

  ExpectSameRangesAsLegacy(sample, /*enters_open_shadow_roots=*/true);
}

// CreateRange() and CreateRangeForSelectionIndexing() don't enter open shadow
// roots, so for them the runs before a range include a shadow host's slotted
// text, but not its shadow text and line breaks.
TEST_F(PlainTextRangeTest, CreateRangeNotEnteringOpenShadowRootMatchesLegacy) {
  SetContentEditableWithShadowHost(ShadowRootMode::kOpen);
  const Element& sample = *GetElementById("sample");

  // Check that these functions don't enter the shadow root: the offsets [2, 4)
  // are "li" in the slotted text.
  Node* const slotted_text = GetElementById("host")->firstChild();
  const EphemeralRange slotted_li(Position(slotted_text, 0),
                                  Position(slotted_text, 2));
  const PlainTextRange plain_text_range(2, 4);
  EXPECT_EQ(slotted_li, plain_text_range.CreateRange(sample));
  EXPECT_EQ(slotted_li,
            plain_text_range.CreateRangeForSelectionIndexing(sample));

  ExpectSameRangesAsLegacy(sample, /*enters_open_shadow_roots=*/false);
}

// No CreateRange*() function enters closed shadow roots, so for all of them the
// runs before a range include a shadow host's slotted text, but not its shadow
// text and line breaks.
TEST_F(PlainTextRangeTest, CreateRangeWithClosedShadowRootMatchesLegacy) {
  SetContentEditableWithShadowHost(ShadowRootMode::kClosed);
  const Element& sample = *GetElementById("sample");

  // Check that no function enters the shadow root: the offsets [2, 4) are "li"
  // in the slotted text for all of them.
  Node* const slotted_text = GetElementById("host")->firstChild();
  const EphemeralRange slotted_li(Position(slotted_text, 0),
                                  Position(slotted_text, 2));
  const PlainTextRange plain_text_range(2, 4);
  EXPECT_EQ(slotted_li, plain_text_range.CreateRange(sample));
  EXPECT_EQ(slotted_li, plain_text_range.CreateRangeForSelection(sample));
  EXPECT_EQ(slotted_li,
            plain_text_range.CreateRangeForSelectionIndexing(sample));

  ExpectSameRangesAsLegacy(sample, /*enters_open_shadow_roots=*/true);
  ExpectSameRangesAsLegacy(sample, /*enters_open_shadow_roots=*/false);
}

// Collapsed whitespace isn't part of the plain text, so plain text offsets
// differ from DOM offsets.
TEST_F(PlainTextRangeTest, CreateRangeWithCollapsibleWhitespaceMatchesLegacy) {
  ExpectSameRangesAsLegacyInContentEditable("a   b    c");
  ExpectSameRangesAsLegacyInContentEditable(
      "<span>ab</span><span>cd</span>   <span> ef </span>  <span>gh</span>");
  ExpectSameRangesAsLegacyInContentEditable(
      "\n  <div>  first   line  </div>\n"
      "  <div>second <span> line </span> </div>\n");
}

// Scopes with no plain text: empty, only collapsible whitespace and only
// non-displayed content.
TEST_F(PlainTextRangeTest, CreateRangeWithoutVisibleTextMatchesLegacy) {
  ExpectSameRangesAsLegacyInContentEditable("");
  ExpectSameRangesAsLegacyInContentEditable("   ");
  ExpectSameRangesAsLegacyInContentEditable(
      "<span style='display:none'>hidden</span>");
}

// Plain text offsets count UTF-16 code units, so they can point into a
// surrogate pair or between a character and its combining marks.
TEST_F(PlainTextRangeTest,
       CreateRangeWithSurrogatePairsAndCombiningMarksMatchesLegacy) {
  ExpectSameRangesAsLegacyInContentEditable(
      base::StrCat({"a", kSurrogatePair, "b", kSurrogatePair, kSurrogatePair,
                    "c", kSurrogatePair, "d"}));
  // U+0301 COMBINING ACUTE ACCENT and U+0308 COMBINING DIAERESIS.
  ExpectSameRangesAsLegacyInContentEditable("ae\xCC\x81i\xCC\x88o");
}

}  // namespace blink
