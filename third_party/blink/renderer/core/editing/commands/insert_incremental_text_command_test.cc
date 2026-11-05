// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/commands/insert_incremental_text_command.h"

#include <unicode/utf16.h>

#include <algorithm>
#include <string>

#include "base/functional/function_ref.h"
#include "base/strings/strcat.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/editing/plain_text_range.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

namespace {

// U+20B9F, which is a surrogate pair in UTF-16.
constexpr char kSurrogatePair[] = "\xF0\xA0\xAE\x9F";

}  // namespace

class InsertIncrementalTextCommandTest : public EditingTestBase {
 protected:
  // Checks that with FastCommonGraphemeClusterPrefixLength, an incremental
  // insertion inserts the same text and changes the DOM and the selection in
  // the same way as without it, for every selection of up to three characters
  // in the editable that `set_up` sets up and returns, with
  // FastPlainTextRangeCreateRange both disabled and enabled. `set_up` is
  // called before every insertion, as insertions change the DOM.
  void ExpectSameReplacementsAsLegacy(
      base::FunctionRef<const ContainerNode&()> set_up) {
    const wtf_size_t length =
        TextIterator::RangeLength(EphemeralRange::RangeOfContents(set_up()));
    // Track that some selection was replaced, so the EXPECT_EQ()s below can't
    // all pass just because every selection had a line break.
    bool replaced = false;
    for (const bool fast_plain_text_range : {false, true}) {
      ScopedFastPlainTextRangeCreateRangeForTest scoped_plain_text_range(
          fast_plain_text_range);
      for (wtf_size_t start = 0; start < length; ++start) {
        for (wtf_size_t end = start + 1; end <= std::min(start + 3, length);
             ++end) {
          for (const bool append : {false, true}) {
            SCOPED_TRACE(testing::Message()
                         << "fast_plain_text_range=" << fast_plain_text_range
                         << " start=" << start << " end=" << end
                         << " append=" << append);
            std::string legacy;
            {
              ScopedFastCommonGraphemeClusterPrefixLengthForTest legacy_feature(
                  false);
              legacy = ReplaceIncrementally(set_up(), start, end, append);
            }
            const std::string fast =
                ReplaceIncrementally(set_up(), start, end, append);
            replaced |= !fast.empty();
            EXPECT_EQ(legacy, fast);
          }
        }
      }
    }
    EXPECT_TRUE(replaced);
  }

  void ExpectSameReplacementsAsLegacyInContentEditable(
      const std::string& html) {
    SCOPED_TRACE(html);
    ExpectSameReplacementsAsLegacy([&]() -> const ContainerNode& {
      SetBodyContent(
          base::StrCat({"<div contenteditable id='sample'>", html, "</div>"}));
      return *GetElementById("sample");
    });
  }

  // `html` has a text control with the id "sample".
  void ExpectSameReplacementsAsLegacyInTextControl(const std::string& html) {
    SCOPED_TRACE(html);
    ExpectSameReplacementsAsLegacy([&]() -> const ContainerNode& {
      SetBodyContent(html);
      return *To<TextControlElement>(GetElementById("sample"))
                  ->InnerEditorElement();
    });
  }

  // Sets up a contenteditable with the id "sample" that contains a shadow host.
  // The host's `mode` shadow root has text and line breaks around a slot for
  // the host's text. Returns the contenteditable.
  const Element& SetContentEditableWithShadowHost(ShadowRootMode mode) {
    SetBodyContent(
        "<div contenteditable id='sample'>ab<span id='host'>light</span>cd"
        "</div>");
    GetElementById("host")
        ->AttachShadowRootForTesting(mode)
        .SetInnerHTMLWithoutTrustedTypes("wx<br><slot></slot><br>yz");
    UpdateAllLifecyclePhasesForTest();
    return *GetElementById("sample");
  }

  // Sets up a contenteditable containing `html` in a `mode` shadow root, as in
  // a custom element. Returns the contenteditable.
  const Element& SetContentEditableInShadowTree(ShadowRootMode mode,
                                                const std::string& html) {
    SetBodyContent("<div id='host'></div>");
    ShadowRoot& shadow_root =
        GetElementById("host")->AttachShadowRootForTesting(mode);
    shadow_root.SetInnerHTMLWithoutTrustedTypes(
        String::FromUtf8(std::string_view(base::StrCat(
            {"<div contenteditable id='sample'>", html, "</div>"}))));
    UpdateAllLifecyclePhasesForTest();
    return *shadow_root.getElementById(AtomicString("sample"));
  }

 private:
  // Selects the text between the plain text offsets `start` and `end` of
  // `scope`, and replaces it incrementally with new text that starts with the
  // same text: all of it, followed by "x", if `append`, or all but its last
  // code point, followed by "x", otherwise. Returns the text the command
  // inserted, which omits the common prefix and suffix of the selected and new
  // text, followed by the resulting flat tree, which includes shadow trees and
  // the inner editors of text controls, with the selection marked. Returns an
  // empty string without replacing the text if it spans a line break,
  // including one in a shadow tree: compositions don't span line breaks, and
  // replacing such text merges paragraphs.
  std::string ReplaceIncrementally(const ContainerNode& scope,
                                   wtf_size_t start,
                                   wtf_size_t end,
                                   bool append) {
    const EphemeralRange range = PlainTextRange(start, end).CreateRange(scope);
    Selection().SetSelection(
        SelectionInDomTree::Builder().SetBaseAndExtent(range).Build(),
        SetSelectionOptions());
    const String old_text = PlainText(
        range, TextIteratorBehavior::EmitsObjectReplacementCharacterBehavior());
    if (old_text.contains('\n') ||
        PlainText(ToEphemeralRangeInFlatTree(range)).contains('\n')) {
      return std::string();
    }
    wtf_size_t kept_length = old_text.length();
    if (!append && kept_length) {
      --kept_length;
      if (kept_length && U16_IS_TRAIL(old_text[kept_length]) &&
          U16_IS_LEAD(old_text[kept_length - 1])) {
        --kept_length;
      }
    }
    const String new_text = old_text.substr(0, kept_length) + "x";
    CompositeEditCommand* const command =
        MakeGarbageCollected<InsertIncrementalTextCommand>(
            GetDocument(), new_text,
            EditCommand::PasswordEchoBehavior::kDoNotEcho);
    command->Apply();
    return base::StrCat(
        {"inserted '", command->TextDataForInputEvent().Utf8(), "' into ",
         GetSelectionTextInFlatTreeFromBody(ConvertToSelectionInFlatTree(
             Selection().GetSelectionInDomTree()))});
  }

  // Turns FastCommonGraphemeClusterPrefixLength on for every test, so that no
  // test depends on the flag's default. ExpectSameReplacementsAsLegacy() turns
  // it off to make the legacy replacements.
  ScopedFastCommonGraphemeClusterPrefixLengthForTest scoped_feature_{true};
};

// http://crbug.com/706166
TEST_F(InsertIncrementalTextCommandTest, SurrogatePairsReplace) {
  SetBodyContent("<div id=sample contenteditable><a>a</a>b&#x1F63A;</div>");
  Element* const sample = GetElementById("sample");
  const String new_text(Vector<UChar>{0xD83D, 0xDE38});  // U+1F638
  Selection().SetSelection(SelectionInDomTree::Builder()
                               .Collapse(Position(sample->lastChild(), 1))
                               .Extend(Position(sample->lastChild(), 3))
                               .Build(),
                           SetSelectionOptions());
  CompositeEditCommand* const command =
      MakeGarbageCollected<InsertIncrementalTextCommand>(
          GetDocument(), new_text,
          EditCommand::PasswordEchoBehavior::kDoNotEcho);
  command->Apply();

  EXPECT_EQ(String(Vector<UChar>{'b', 0xD83D, 0xDE38}),
            sample->lastChild()->nodeValue())
      << "Replace 'U+D83D U+DE3A (U+1F63A) with 'U+D83D U+DE38'(U+1F638)";
}

TEST_F(InsertIncrementalTextCommandTest, SurrogatePairsNoReplace) {
  SetBodyContent("<div id=sample contenteditable><a>a</a>b&#x1F63A;</div>");
  Element* const sample = GetElementById("sample");
  const String new_text(Vector<UChar>{0xD83D, 0xDE3A});  // U+1F63A
  Selection().SetSelection(SelectionInDomTree::Builder()
                               .Collapse(Position(sample->lastChild(), 1))
                               .Extend(Position(sample->lastChild(), 3))
                               .Build(),
                           SetSelectionOptions());
  CompositeEditCommand* const command =
      MakeGarbageCollected<InsertIncrementalTextCommand>(
          GetDocument(), new_text,
          EditCommand::PasswordEchoBehavior::kDoNotEcho);
  command->Apply();

  EXPECT_EQ(String(Vector<UChar>{'b', 0xD83D, 0xDE3A}),
            sample->lastChild()->nodeValue())
      << "Replace 'U+D83D U+DE3A(U+1F63A) with 'U+D83D U+DE3A'(U+1F63A)";
}

// http://crbug.com/706166
TEST_F(InsertIncrementalTextCommandTest, SurrogatePairsTwo) {
  SetBodyContent(
      "<div id=sample contenteditable><a>a</a>b&#x1F63A;&#x1F63A;</div>");
  Element* const sample = GetElementById("sample");
  const String new_text(Vector<UChar>{0xD83D, 0xDE38});  // U+1F638
  Selection().SetSelection(SelectionInDomTree::Builder()
                               .Collapse(Position(sample->lastChild(), 1))
                               .Extend(Position(sample->lastChild(), 5))
                               .Build(),
                           SetSelectionOptions());
  CompositeEditCommand* const command =
      MakeGarbageCollected<InsertIncrementalTextCommand>(
          GetDocument(), new_text,
          EditCommand::PasswordEchoBehavior::kDoNotEcho);
  command->Apply();

  EXPECT_EQ(String(Vector<UChar>{'b', 0xD83D, 0xDE38}),
            sample->lastChild()->nodeValue())
      << "Replace 'U+1F63A U+1F63A with U+1F638";
}

TEST_F(InsertIncrementalTextCommandTest,
       SurrogatePairsReplaceWithPreceedingNonEditableText) {
  SetBodyContent(
      "<div id=sample contenteditable><span "
      "contenteditable='false'>•</span>&#x1F63A;&#x1F638;</div>");
  Element* const sample = GetElementById("sample");
  const String new_text(Vector<UChar>{0xD83D, 0xDE38});  // U+1F638
  Selection().SetSelection(SelectionInDomTree::Builder()
                               .Collapse(Position(sample->lastChild(), 2))
                               .Extend(Position(sample->lastChild(), 4))
                               .Build(),
                           SetSelectionOptions());
  CompositeEditCommand* const command =
      MakeGarbageCollected<InsertIncrementalTextCommand>(
          GetDocument(), new_text,
          EditCommand::PasswordEchoBehavior::kDoNotEcho);
  command->Apply();

  EXPECT_EQ(String(Vector<UChar>{0xD83D, 0xDE3A, 0xD83D, 0xDE38}),
            sample->lastChild()->nodeValue())
      << "Replace U+1F638 with U+1F638";
}

// An incremental insertion only inserts the part of the new text that isn't in
// a common prefix or suffix with the selected text, adjusted to grapheme
// cluster boundaries. Adjusting the common prefix finds where the selection's
// container starts in its parent, which FastCommonGraphemeClusterPrefixLength
// does without computing the positions in the runs before it in other
// containers, like the newlines emitted for <br>s.
TEST_F(InsertIncrementalTextCommandTest,
       ReplacementInContentEditableMatchesLegacy) {
  // Line breaks, including at the start and the end.
  ExpectSameReplacementsAsLegacyInContentEditable("<br>ab<br><br>cd<br>");
  // Blocks, whose separators are emitted only after text.
  ExpectSameReplacementsAsLegacyInContentEditable("<p>first</p><p>second</p>");
  ExpectSameReplacementsAsLegacyInContentEditable(
      "<div>first</div><div>second</div>");
  // A replaced element, an inline element and a table.
  ExpectSameReplacementsAsLegacyInContentEditable(
      "ab<img width='10' height='10'>cd<b>ef</b>"
      "<table><tr><td>g</td><td>h</td></tr></table>ij");
}

// Text controls don't use incremental insertions, which are only for richly
// editable content, but a <textarea>'s inner editor has a <br> for every line
// break, like the contenteditables that FastCommonGraphemeClusterPrefixLength
// speeds up.
TEST_F(InsertIncrementalTextCommandTest,
       ReplacementInTextControlMatchesLegacy) {
  ExpectSameReplacementsAsLegacyInTextControl(
      "<textarea id='sample'>ab\n\ncd\nef  gh\n\n</textarea>");
  ExpectSameReplacementsAsLegacyInTextControl(
      base::StrCat({"<textarea id='sample'>", kSurrogatePair, "a",
                    kSurrogatePair, "\nb", kSurrogatePair, "</textarea>"}));
  ExpectSameReplacementsAsLegacyInTextControl(
      "<input id='sample' value='hello   world'>");
  ExpectSameReplacementsAsLegacyInTextControl(
      base::StrCat({"<input id='sample' value='a", kSurrogatePair, "b'>"}));
}

// The text iterator that finds where the selection's container starts doesn't
// enter shadow roots, open or closed, so the runs before a selection after a
// shadow host include the host's slotted text, but not its shadow text and
// line breaks.
TEST_F(InsertIncrementalTextCommandTest,
       ReplacementWithShadowHostMatchesLegacy) {
  for (const ShadowRootMode mode :
       {ShadowRootMode::kOpen, ShadowRootMode::kClosed}) {
    SCOPED_TRACE(mode == ShadowRootMode::kOpen ? "open" : "closed");
    ExpectSameReplacementsAsLegacy([&]() -> const ContainerNode& {
      return SetContentEditableWithShadowHost(mode);
    });
  }
}

TEST_F(InsertIncrementalTextCommandTest, ReplacementInShadowTreeMatchesLegacy) {
  for (const ShadowRootMode mode :
       {ShadowRootMode::kOpen, ShadowRootMode::kClosed}) {
    SCOPED_TRACE(mode == ShadowRootMode::kOpen ? "open" : "closed");
    ExpectSameReplacementsAsLegacy([&]() -> const ContainerNode& {
      return SetContentEditableInShadowTree(mode, "<br>ab<br><br>cd<br>");
    });
  }
}

// Collapsed whitespace isn't part of the plain text, so plain text offsets
// differ from DOM offsets. When the first character of the selection's
// container is collapsed, no character starts at the start of the container,
// so finding where it starts goes through all of its parent's text.
TEST_F(InsertIncrementalTextCommandTest,
       ReplacementWithCollapsibleWhitespaceMatchesLegacy) {
  ExpectSameReplacementsAsLegacyInContentEditable("a   b    c");
  ExpectSameReplacementsAsLegacyInContentEditable(
      "<span>ab</span><span>cd</span>   <span> ef </span>  <span>gh</span>");
  ExpectSameReplacementsAsLegacyInContentEditable(
      "\n  <div>  first   line  </div>\n"
      "  <div>second <span> line </span> </div>\n");
  ExpectSameReplacementsAsLegacyInContentEditable("a<br> bc <br>  d");
  ExpectSameReplacementsAsLegacyInContentEditable("a b<br>c  d");
}

// Editables with no plain text have nothing to replace, so this checks text
// next to elements with no plain text: only collapsible whitespace or only
// non-displayed content.
TEST_F(InsertIncrementalTextCommandTest,
       ReplacementNextToContentWithoutVisibleTextMatchesLegacy) {
  ExpectSameReplacementsAsLegacyInContentEditable(
      "ab<span>   </span>cd<br><span> </span>ef");
  ExpectSameReplacementsAsLegacyInContentEditable(
      "ab<span style='display:none'>hidden</span>cd<br>"
      "<span style='display:none'>hidden</span>ef");
}

// Plain text offsets count UTF-16 code units, so a selection can start or end
// inside a surrogate pair or between a character and its combining marks, and
// the common prefix of the selected and new text has to be adjusted to a
// grapheme cluster boundary.
TEST_F(InsertIncrementalTextCommandTest,
       ReplacementWithSurrogatePairsAndCombiningMarksMatchesLegacy) {
  ExpectSameReplacementsAsLegacyInContentEditable(
      base::StrCat({"a", kSurrogatePair, "b", kSurrogatePair, kSurrogatePair,
                    "c", kSurrogatePair, "d"}));
  // U+0301 COMBINING ACUTE ACCENT and U+0308 COMBINING DIAERESIS, also after
  // line breaks and in an inline element.
  ExpectSameReplacementsAsLegacyInContentEditable("ae\xCC\x81i\xCC\x88o");
  ExpectSameReplacementsAsLegacyInContentEditable(
      base::StrCat({"<br>ae\xCC\x81<br>i\xCC\x88u<br>", kSurrogatePair, "c"}));
  ExpectSameReplacementsAsLegacyInContentEditable(
      "ab<img width='10' height='10'>cd<b>e\xCC\x81"
      "f</b>gh");
}

}  // namespace blink
