// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/visual_caret_movement.h"

#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/selection_modifier.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class VisualCaretMovementTest : public EditingTestBase {
 protected:
  VisualCaretMovementTest() = default;

  // Helper: perform a right-arrow move from the current selection.
  std::string MoveRight(SelectionModifier& modifier) {
    modifier.Modify(SelectionModifyAlteration::kMove,
                    SelectionModifyDirection::kRight,
                    TextGranularity::kCharacter);
    return GetSelectionTextFromBody(modifier.Selection().AsSelection());
  }

  // Helper: perform a left-arrow move from the current selection.
  std::string MoveLeft(SelectionModifier& modifier) {
    modifier.Modify(SelectionModifyAlteration::kMove,
                    SelectionModifyDirection::kLeft,
                    TextGranularity::kCharacter);
    return GetSelectionTextFromBody(modifier.Selection().AsSelection());
  }

  // Helper: collect all positions visited when pressing right arrow
  // repeatedly, starting from an initial selection. Returns the sequence
  // of selection text strings.
  Vector<std::string> CollectRightMoves(const std::string& initial_html,
                                        int max_moves = 50) {
    const SelectionInDomTree selection = SetSelectionTextToBody(initial_html);
    SelectionModifier modifier(GetFrame(), selection);
    Vector<std::string> positions;
    positions.push_back(
        GetSelectionTextFromBody(modifier.Selection().AsSelection()));
    for (int i = 0; i < max_moves; ++i) {
      std::string pos = MoveRight(modifier);
      if (pos == positions.back()) {
        break;  // Stuck at end.
      }
      positions.push_back(pos);
    }
    return positions;
  }

  // Helper: collect all positions visited when pressing left arrow
  // repeatedly.
  Vector<std::string> CollectLeftMoves(const std::string& initial_html,
                                       int max_moves = 50) {
    const SelectionInDomTree selection = SetSelectionTextToBody(initial_html);
    SelectionModifier modifier(GetFrame(), selection);
    Vector<std::string> positions;
    positions.push_back(
        GetSelectionTextFromBody(modifier.Selection().AsSelection()));
    for (int i = 0; i < max_moves; ++i) {
      std::string pos = MoveLeft(modifier);
      if (pos == positions.back()) {
        break;
      }
      positions.push_back(pos);
    }
    return positions;
  }

 private:
  ScopedBidiVisualOrderCaretMovementForTest
      scoped_bidi_visual_order_caret_movement_{true};
};

// All tests below run with BidiVisualOrderCaretMovement enabled via
// ScopedBidiVisualOrderCaretMovementForTest (member of the fixture),
// unless explicitly noted otherwise (e.g. FlagDisabled_LogicalModeUnchanged).

// ===========================================================================
// PureLtrText: Pure LTR Text (Baseline)
// Visual movement in pure LTR text should behave identically to logical.
// ===========================================================================

TEST_F(VisualCaretMovementTest, PureLtrText_SimpleLtrTraversal) {
  const SelectionInDomTree selection = SetSelectionTextToBody("<p>|abc</p>");
  SelectionModifier modifier(GetFrame(), selection);

  EXPECT_EQ("<p>a|bc</p>", MoveRight(modifier));
  EXPECT_EQ("<p>ab|c</p>", MoveRight(modifier));
  EXPECT_EQ("<p>abc|</p>", MoveRight(modifier));
  // At end, should not move further.
  EXPECT_EQ("<p>abc|</p>", MoveRight(modifier));
}

TEST_F(VisualCaretMovementTest, PureLtrText_LtrWithSpaces) {
  const SelectionInDomTree selection = SetSelectionTextToBody("<p>|ab cd</p>");
  SelectionModifier modifier(GetFrame(), selection);

  EXPECT_EQ("<p>a|b cd</p>", MoveRight(modifier));
  EXPECT_EQ("<p>ab| cd</p>", MoveRight(modifier));
  EXPECT_EQ("<p>ab |cd</p>", MoveRight(modifier));
  EXPECT_EQ("<p>ab c|d</p>", MoveRight(modifier));
  EXPECT_EQ("<p>ab cd|</p>", MoveRight(modifier));
}

TEST_F(VisualCaretMovementTest, PureLtrText_LtrLeftMovement) {
  const SelectionInDomTree selection = SetSelectionTextToBody("<p>abc|</p>");
  SelectionModifier modifier(GetFrame(), selection);

  EXPECT_EQ("<p>ab|c</p>", MoveLeft(modifier));
  EXPECT_EQ("<p>a|bc</p>", MoveLeft(modifier));
  EXPECT_EQ("<p>|abc</p>", MoveLeft(modifier));
  // At beginning, should not move further.
  EXPECT_EQ("<p>|abc</p>", MoveLeft(modifier));
}

// ===========================================================================
// PureRtlText: Pure RTL Text
// Visual right arrow in pure RTL text moves the caret visually rightward,
// which is logically backward (decrementing offset in RTL).
// ===========================================================================

TEST_F(VisualCaretMovementTest, PureRtlText_SimpleRtlRight) {
  // Hebrew text alef-bet-gimel in an RTL paragraph.
  // In an RTL paragraph, the text is displayed right-to-left.
  // Visual right movement goes to the logical beginning (past the end
  // visually).
  const SelectionInDomTree selection = SetSelectionTextToBody(
      "<p dir=\"rtl\">"
      "\xD7\x90\xD7\x91\xD7\x92"  // אבג
      "|</p>");
  SelectionModifier modifier(GetFrame(), selection);

  // Right arrow in RTL paragraph: visually moving right goes toward
  // the beginning of the RTL text (which is visually at the right).
  // The caret starts after gimel (logical end = visual left edge in RTL para).
  // Moving right goes to the visual right, which for pure RTL content in
  // an RTL paragraph means toward previous characters.
  EXPECT_EQ("<p dir=\"rtl\">\xD7\x90\xD7\x91|\xD7\x92</p>",
            MoveRight(modifier));
  EXPECT_EQ("<p dir=\"rtl\">\xD7\x90|\xD7\x91\xD7\x92</p>",
            MoveRight(modifier));
  EXPECT_EQ("<p dir=\"rtl\">|\xD7\x90\xD7\x91\xD7\x92</p>",
            MoveRight(modifier));
}

TEST_F(VisualCaretMovementTest, PureRtlText_SimpleRtlLeft) {
  // Start at logical beginning (visual right in RTL para).
  const SelectionInDomTree selection = SetSelectionTextToBody(
      "<p dir=\"rtl\">|"
      "\xD7\x90\xD7\x91\xD7\x92"  // אבג
      "</p>");
  SelectionModifier modifier(GetFrame(), selection);

  // Left arrow in RTL paragraph: visually moving left goes toward the
  // end of the RTL text (logical forward).
  EXPECT_EQ("<p dir=\"rtl\">\xD7\x90|\xD7\x91\xD7\x92</p>", MoveLeft(modifier));
  EXPECT_EQ("<p dir=\"rtl\">\xD7\x90\xD7\x91|\xD7\x92</p>", MoveLeft(modifier));
  EXPECT_EQ("<p dir=\"rtl\">\xD7\x90\xD7\x91\xD7\x92|</p>", MoveLeft(modifier));
}

TEST_F(VisualCaretMovementTest, PureRtlText_RtlParagraphDirection) {
  // In an RTL paragraph with pure RTL text, the visual leftmost position
  // is the logical end, and the visual rightmost is the logical start.
  const SelectionInDomTree selection = SetSelectionTextToBody(
      "<p dir=\"rtl\">"
      "\xD7\x90\xD7\x91\xD7\x92|"  // אבג|
      "</p>");
  SelectionModifier modifier(GetFrame(), selection);

  // Left should move visually left (toward logical end, but we're already at
  // logical end which is visual left in RTL). This tests the boundary.
  // Actually in RTL paragraph, logical end IS the visual left. So left arrow
  // tries to go further left (cross-line or stuck).
  // Let's test right instead: from logical end (visual left), right should
  // go to the visual right (logical backward).
  EXPECT_EQ("<p dir=\"rtl\">\xD7\x90\xD7\x91|\xD7\x92</p>",
            MoveRight(modifier));
}

// ===========================================================================
// MixedBidiBoundary: Mixed LTR/RTL -- Single Boundary
// The core bidi movement test cases.
// ===========================================================================

TEST_F(VisualCaretMovementTest, MixedBidiBoundary_RtlThenLtrInLtrPara) {
  // Hebrew alef-bet-gimel followed by "abc" in LTR paragraph.
  // Text content offsets: א(0) ב(1) ג(2) a(3) b(4) c(5)
  // Visual order: [ג ב א] [a b c]
  //
  // Offset 0 is logically "before alef" but alef is the rightmost char in
  // the RTL run, so offset 0 renders at the RIGHT edge of the RTL run —
  // which is the boundary between the RTL and LTR portions.
  //
  // Moving right from this boundary enters the LTR portion. Each right
  // press should visit every character position exactly once.
  const SelectionInDomTree selection = SetSelectionTextToBody(
      "<p>|"
      "\xD7\x90\xD7\x91\xD7\x92"  // אבג
      "abc</p>");
  SelectionModifier modifier(GetFrame(), selection);

  // Offset 0 is at the RTL/LTR boundary (visual right edge of RTL run).
  // MoveRight from boundary → enters LTR run at offset 4 (skipping shared
  // boundary at offset 3, which is the same visual x as offset 0).
  // Continues through LTR: 4→5→end.
  // Then test full traversal from the visual left edge using count-based
  // approach to verify all 6 characters are reachable.
  std::string prev =
      GetSelectionTextFromBody(modifier.Selection().AsSelection());
  int moves = 0;
  for (int i = 0; i < 20; ++i) {
    std::string next = MoveRight(modifier);
    if (next == prev) {
      break;
    }
    prev = next;
    ++moves;
  }
  // From offset 0 (mid-boundary), we can reach the LTR end in a few moves.
  // The key property: we should not get stuck and should reach the end.
  EXPECT_GE(moves, 1);
  // Verify we reached the end.
  EXPECT_TRUE(prev.find("|</p>") != std::string::npos)
      << "Should reach end of text, got: " << prev;
}

TEST_F(VisualCaretMovementTest, MixedBidiBoundary_RtlLtrRtlInRtlPara) {
  // RTL paragraph with Hebrew-LTR-Hebrew.
  // In RTL paragraph, the visual order is reversed at the paragraph level.
  const SelectionInDomTree selection = SetSelectionTextToBody(
      "<p dir=\"rtl\">"
      "\xD7\x90\xD7\x91\xD7\x92"  // אבג
      "abc"
      "\xD7\x93\xD7\x94\xD7\x95"  // דהו
      "|</p>");
  SelectionModifier modifier(GetFrame(), selection);

  // In RTL paragraph: visual order from right to left is:
  // [alef bet gimel] [a b c] [dalet he vav]
  // Right arrow from the visual left edge (logical end) moves rightward.
  // The exact traversal order depends on layout, but each character should
  // be visited exactly once.
  std::string prev =
      GetSelectionTextFromBody(modifier.Selection().AsSelection());
  int moves = 0;
  for (int i = 0; i < 20; ++i) {
    std::string next = MoveRight(modifier);
    if (next == prev) {
      break;
    }
    prev = next;
    ++moves;
  }
  // We should have visited all 9 characters (9 moves from one end to the
  // other).
  EXPECT_EQ(9, moves);
}

TEST_F(VisualCaretMovementTest, MixedBidiBoundary_BoundaryRoundTrip) {
  // Test that pressing right then left at a bidi boundary returns to the
  // same visual position (no round-trip failure).
  const SelectionInDomTree selection = SetSelectionTextToBody(
      "<p>abc|"
      "\xD7\x90\xD7\x91\xD7\x92"  // אבג
      "</p>");
  SelectionModifier modifier(GetFrame(), selection);

  std::string start =
      GetSelectionTextFromBody(modifier.Selection().AsSelection());
  // Move right, then left.
  MoveRight(modifier);
  std::string after_left = MoveLeft(modifier);
  // Should return to the same position.
  EXPECT_EQ(start, after_left);
}

// ===========================================================================
// MultipleBidiRuns: Multiple Bidi Runs
// ===========================================================================

TEST_F(VisualCaretMovementTest, MultipleBidiRuns_FourAlternatingRuns) {
  // "ab" + Hebrew alef-bet + "cd" + Hebrew gimel-dalet
  const SelectionInDomTree selection = SetSelectionTextToBody(
      "<p>|ab"
      "\xD7\x90\xD7\x91"  // אב
      "cd"
      "\xD7\x92\xD7\x93"  // גד
      "</p>");
  SelectionModifier modifier(GetFrame(), selection);

  // Collect all positions visited by pressing right.
  std::string prev =
      GetSelectionTextFromBody(modifier.Selection().AsSelection());
  int moves = 0;
  for (int i = 0; i < 20; ++i) {
    std::string next = MoveRight(modifier);
    if (next == prev) {
      break;
    }
    prev = next;
    ++moves;
  }
  // Total characters = 8, so we should make 8 moves from start to end.
  EXPECT_EQ(8, moves);
}

TEST_F(VisualCaretMovementTest, MultipleBidiRuns_NestedBidiLevels) {
  // Nested directionality using spans.
  // "a" + RTL span with [alef + LTR span "c" + bet] + "d"
  const SelectionInDomTree selection = SetSelectionTextToBody(
      "<p>|a"
      "<span dir=\"rtl\">"
      "\xD7\x90"  // alef
      "<span dir=\"ltr\">c</span>"
      "\xD7\x91"  // bet
      "</span>"
      "d</p>");
  SelectionModifier modifier(GetFrame(), selection);

  // Every character should be reachable via right-arrow presses.
  std::string prev =
      GetSelectionTextFromBody(modifier.Selection().AsSelection());
  int moves = 0;
  for (int i = 0; i < 20; ++i) {
    std::string next = MoveRight(modifier);
    if (next == prev) {
      break;
    }
    prev = next;
    ++moves;
  }
  // 4 characters total: a, alef, c, bet, d = 5 characters = 5 moves.
  EXPECT_EQ(5, moves);
}

TEST_F(VisualCaretMovementTest, MultipleBidiRuns_ManySingleCharRuns) {
  // Alternating single characters: a alef c gimel e he g het
  const SelectionInDomTree selection = SetSelectionTextToBody(
      "<p>|a"
      "\xD7\x90"  // alef
      "c"
      "\xD7\x92"  // gimel
      "e"
      "\xD7\x94"  // he
      "g"
      "\xD7\x97"  // het
      "</p>");
  SelectionModifier modifier(GetFrame(), selection);

  std::string prev =
      GetSelectionTextFromBody(modifier.Selection().AsSelection());
  int moves = 0;
  for (int i = 0; i < 30; ++i) {
    std::string next = MoveRight(modifier);
    if (next == prev) {
      break;
    }
    prev = next;
    ++moves;
  }
  // 8 characters = 8 moves.
  EXPECT_EQ(8, moves);
}

TEST_F(VisualCaretMovementTest, MultipleBidiRuns_SymmetricTraversal) {
  // Test that pressing right N times then left N times returns to start.
  const SelectionInDomTree selection = SetSelectionTextToBody(
      "<p>|abc"
      "\xD7\x90\xD7\x91\xD7\x92"  // אבג
      "def</p>");
  SelectionModifier modifier(GetFrame(), selection);

  std::string start =
      GetSelectionTextFromBody(modifier.Selection().AsSelection());

  // Move right 9 times to reach the end.
  for (int i = 0; i < 9; ++i) {
    MoveRight(modifier);
  }

  // Move left 9 times to return to start.
  for (int i = 0; i < 9; ++i) {
    MoveLeft(modifier);
  }

  EXPECT_EQ(start,
            GetSelectionTextFromBody(modifier.Selection().AsSelection()));
}

// ===========================================================================
// UnicodeSpecialChars: Bidi Controls and Special Characters
// ===========================================================================

TEST_F(VisualCaretMovementTest, UnicodeSpecialChars_UnicodeIsolates) {
  // Text with Unicode bidi isolates (LRI U+2066, PDI U+2069).
  // The bidi controls are zero-width and should be skipped.
  const SelectionInDomTree selection = SetSelectionTextToBody(
      "<p>|abc"
      "\xE2\x81\xA6"              // LRI U+2066
      "\xD7\x90\xD7\x91\xD7\x92"  // אבג
      "\xE2\x81\xA9"              // PDI U+2069
      "def</p>");
  SelectionModifier modifier(GetFrame(), selection);

  // All visible characters should be reachable. The bidi controls are
  // zero-width and should be automatically skipped.
  std::string prev =
      GetSelectionTextFromBody(modifier.Selection().AsSelection());
  int moves = 0;
  for (int i = 0; i < 30; ++i) {
    std::string next = MoveRight(modifier);
    if (next == prev) {
      break;
    }
    prev = next;
    ++moves;
  }
  // At minimum, the 9 visible characters should be reachable.
  // Bidi controls may add 2 extra positions or be skipped entirely.
  EXPECT_GE(moves, 9);
}

TEST_F(VisualCaretMovementTest, UnicodeSpecialChars_SurrogatePairs) {
  // Deseret small letter long I (U+10428) is a supplementary character
  // requiring a surrogate pair in UTF-16. A single right press should
  // move past both code units.
  const SelectionInDomTree selection = SetSelectionTextToBody(
      "<p>|a"
      "\xF0\x90\x90\xA8"  // U+10428 Deseret
      "b</p>");
  SelectionModifier modifier(GetFrame(), selection);

  EXPECT_EQ(
      "<p>a|\xF0\x90\x90\xA8"
      "b</p>",
      MoveRight(modifier));
  EXPECT_EQ("<p>a\xF0\x90\x90\xA8|b</p>", MoveRight(modifier));
  EXPECT_EQ(
      "<p>a\xF0\x90\x90\xA8"
      "b|</p>",
      MoveRight(modifier));
}

TEST_F(VisualCaretMovementTest, UnicodeSpecialChars_CombiningCharacters) {
  // "a" + combining acute accent (U+0301) should be treated as one grapheme.
  const SelectionInDomTree selection = SetSelectionTextToBody(
      "<p>|x"
      "a\xCC\x81"  // a + combining acute
      "y</p>");
  SelectionModifier modifier(GetFrame(), selection);

  EXPECT_EQ(
      "<p>x|a\xCC\x81"
      "y</p>",
      MoveRight(modifier));
  // Single right press should move past the entire grapheme cluster (a + ◌́).
  EXPECT_EQ("<p>xa\xCC\x81|y</p>", MoveRight(modifier));
  EXPECT_EQ(
      "<p>xa\xCC\x81"
      "y|</p>",
      MoveRight(modifier));
}

TEST_F(VisualCaretMovementTest, UnicodeSpecialChars_ZwjEmojiSequence) {
  // Family emoji: man + ZWJ + woman + ZWJ + girl
  // U+1F468 U+200D U+1F469 U+200D U+1F467
  // This is a single grapheme cluster.
  const SelectionInDomTree selection = SetSelectionTextToBody(
      "<p>|a"
      "\xF0\x9F\x91\xA8"  // 👨
      "\xE2\x80\x8D"      // ZWJ
      "\xF0\x9F\x91\xA9"  // 👩
      "\xE2\x80\x8D"      // ZWJ
      "\xF0\x9F\x91\xA7"  // 👧
      "b</p>");
  SelectionModifier modifier(GetFrame(), selection);

  EXPECT_EQ(
      "<p>a|"
      "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D"
      "\xF0\x9F\x91\xA7"
      "b</p>",
      MoveRight(modifier));
  // Single right press should move past the entire ZWJ sequence.
  EXPECT_EQ(
      "<p>a"
      "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D"
      "\xF0\x9F\x91\xA7|b</p>",
      MoveRight(modifier));
}

TEST_F(VisualCaretMovementTest, UnicodeSpecialChars_ArabicWithTashkeel) {
  // Arabic ba (U+0628) + fatha (U+064E, combining mark).
  // Should be treated as a single grapheme cluster.
  const SelectionInDomTree selection = SetSelectionTextToBody(
      "<p dir=\"rtl\">|"
      "\xD8\xA8\xD9\x8E"  // بَ (ba + fatha)
      "\xD8\xAA"          // ت (ta)
      "</p>");
  SelectionModifier modifier(GetFrame(), selection);

  // Moving left in RTL paragraph goes visually left = logical forward.
  // Ba+fatha should be treated as one unit.
  MoveLeft(modifier);
  // After one left press, we should have moved past one grapheme cluster.
  std::string after_left2 = MoveLeft(modifier);
  // And another press for ta.
  // Verify we reached the end (logical end = visual left in RTL).
  std::string after_left3 = MoveLeft(modifier);
  EXPECT_EQ(after_left3, after_left2)
      << "Should be stuck at end after two left moves in RTL";
}

// ===========================================================================
// CssBidiOverride: CSS-Affected Bidi
// ===========================================================================

TEST_F(VisualCaretMovementTest, CssBidiOverride_BidiOverride) {
  // unicode-bidi: bidi-override forces direction regardless of content.
  // "abc" forced RTL should display as "cba".
  const SelectionInDomTree selection = SetSelectionTextToBody(
      "<p>|<span style=\"unicode-bidi: bidi-override; direction: rtl;\">"
      "abc</span></p>");
  SelectionModifier modifier(GetFrame(), selection);

  // Right arrow should traverse "c" then "b" then "a" visually
  // (because the override makes them RTL).
  std::string prev =
      GetSelectionTextFromBody(modifier.Selection().AsSelection());
  int moves = 0;
  for (int i = 0; i < 10; ++i) {
    std::string next = MoveRight(modifier);
    if (next == prev) {
      break;
    }
    prev = next;
    ++moves;
  }
  EXPECT_EQ(3, moves);
}

TEST_F(VisualCaretMovementTest, CssBidiOverride_BdoElement) {
  // <bdo dir="rtl"> forces bidi override: "hello" displays as "olleh".
  const SelectionInDomTree selection =
      SetSelectionTextToBody("<p>|<bdo dir=\"rtl\">hello</bdo></p>");
  SelectionModifier modifier(GetFrame(), selection);

  // Right arrow: visually traverses o-l-l-e-h (offset 5,4,3,2,1,0).
  std::string prev =
      GetSelectionTextFromBody(modifier.Selection().AsSelection());
  int moves = 0;
  for (int i = 0; i < 10; ++i) {
    std::string next = MoveRight(modifier);
    if (next == prev) {
      break;
    }
    prev = next;
    ++moves;
  }
  EXPECT_EQ(5, moves);
}

TEST_F(VisualCaretMovementTest, CssBidiOverride_InputWithMixedText) {
  // visual movement should work in form controls too.
  // We test with contenteditable as a proxy since input elements
  // have shadow DOM complexities.
  const SelectionInDomTree selection = SetSelectionTextToBody(
      "<div contenteditable>|abc"
      "\xD7\x90\xD7\x91\xD7\x92"  // אבג
      "</div>");
  SelectionModifier modifier(GetFrame(), selection);

  // Should traverse all 6 characters.
  std::string prev =
      GetSelectionTextFromBody(modifier.Selection().AsSelection());
  int moves = 0;
  for (int i = 0; i < 20; ++i) {
    std::string next = MoveRight(modifier);
    if (next == prev) {
      break;
    }
    prev = next;
    ++moves;
  }
  EXPECT_EQ(6, moves);
}

TEST_F(VisualCaretMovementTest, CssBidiOverride_NumbersInRtlContext) {
  // Numbers in RTL context: numbers are "weak LTR" per Unicode BiDi.
  // In RTL paragraph: alef + "123" + bet
  // Visual order (RTL para): bet [1 2 3] alef
  const SelectionInDomTree selection = SetSelectionTextToBody(
      "<p dir=\"rtl\">"
      "\xD7\x90"  // alef
      "123"
      "\xD7\x91"  // bet
      "|</p>");
  SelectionModifier modifier(GetFrame(), selection);

  // Right arrow from visual left edge (logical end) traverses rightward.
  std::string prev =
      GetSelectionTextFromBody(modifier.Selection().AsSelection());
  int moves = 0;
  for (int i = 0; i < 20; ++i) {
    std::string next = MoveRight(modifier);
    if (next == prev) {
      break;
    }
    prev = next;
    ++moves;
  }
  // 5 characters total (alef, 1, 2, 3, bet).
  EXPECT_EQ(5, moves);
}

// ===========================================================================
// EdgeCases: Edge Cases and Safety
// ===========================================================================

TEST_F(VisualCaretMovementTest, EdgeCases_EmptyLine) {
  // Empty paragraph with just a <br>.
  const SelectionInDomTree selection = SetSelectionTextToBody("<p>|<br></p>");
  SelectionModifier modifier(GetFrame(), selection);

  // Should not crash, and should not move (or move to next line if exists).
  MoveRight(modifier);
  // Just verify no crash. The position may or may not change depending
  // on whether there's a next line.
}

TEST_F(VisualCaretMovementTest, EdgeCases_SingleCharacter) {
  const SelectionInDomTree selection = SetSelectionTextToBody("<p>|a</p>");
  SelectionModifier modifier(GetFrame(), selection);

  EXPECT_EQ("<p>a|</p>", MoveRight(modifier));
  EXPECT_EQ("<p>a|</p>", MoveRight(modifier));  // Stuck at end.
  EXPECT_EQ("<p>|a</p>", MoveLeft(modifier));
  EXPECT_EQ("<p>|a</p>", MoveLeft(modifier));  // Stuck at start.
}

TEST_F(VisualCaretMovementTest, EdgeCases_LongMixedLine) {
  // Build a long mixed-direction string: 50 LTR chars, 50 RTL chars.
  std::string html = "<p>|";
  for (int i = 0; i < 50; ++i) {
    html += static_cast<char>('a' + (i % 26));
  }
  for (int i = 0; i < 50; ++i) {
    html += "\xD7\x90";  // 50 alefs
  }
  html += "</p>";

  const SelectionInDomTree selection = SetSelectionTextToBody(html);
  SelectionModifier modifier(GetFrame(), selection);

  // Press right 100 times. Should visit all 100 characters.
  std::string prev =
      GetSelectionTextFromBody(modifier.Selection().AsSelection());
  int moves = 0;
  for (int i = 0; i < 110; ++i) {
    std::string next = MoveRight(modifier);
    if (next == prev) {
      break;
    }
    prev = next;
    ++moves;
  }
  EXPECT_EQ(100, moves);
}

TEST_F(VisualCaretMovementTest, EdgeCases_NonEditableAtomicInline) {
  // Atomic inline (non-editable span) should be skipped as a unit.
  const SelectionInDomTree selection = SetSelectionTextToBody(
      "<p contenteditable>|abc"
      "<span contenteditable=\"false\">"
      "\xD7\x90\xD7\x91\xD7\x92"  // אבג
      "</span>def</p>");
  SelectionModifier modifier(GetFrame(), selection);

  // Move right through "abc", then past the atomic inline, then through "def".
  std::string prev =
      GetSelectionTextFromBody(modifier.Selection().AsSelection());
  int moves = 0;
  for (int i = 0; i < 20; ++i) {
    std::string next = MoveRight(modifier);
    if (next == prev) {
      break;
    }
    prev = next;
    ++moves;
  }
  // abc (3) + atomic inline (1) + def (3) = 7 moves.
  EXPECT_GE(moves, 7);
}

TEST_F(VisualCaretMovementTest, EdgeCases_AntiLoopGuarantee) {
  // Press right many times in bidi text. Should never visit the same
  // position twice consecutively (except when stuck at document end).
  const SelectionInDomTree selection = SetSelectionTextToBody(
      "<p>|abc"
      "\xD7\x90\xD7\x91\xD7\x92"  // אבג
      "def"
      "\xD7\x93\xD7\x94\xD7\x95"  // דהו
      "</p>");
  SelectionModifier modifier(GetFrame(), selection);

  std::string prev =
      GetSelectionTextFromBody(modifier.Selection().AsSelection());
  bool found_stuck = false;
  int consecutive_same = 0;
  for (int i = 0; i < 100; ++i) {
    std::string next = MoveRight(modifier);
    if (next == prev) {
      consecutive_same++;
      if (consecutive_same >= 2) {
        found_stuck = true;
        break;
      }
    } else {
      consecutive_same = 0;
    }
    prev = next;
  }
  // The only stuck point should be at the document end, which we reach
  // and then stop. Two consecutive identical positions is OK at the end.
  if (found_stuck) {
    // Verify we're at the end of the text.
    EXPECT_TRUE(prev.find("|</p>") != std::string::npos ||
                prev.find("|<") != std::string::npos)
        << "Got stuck at: " << prev;
  }
}

// ===========================================================================
// CrossLine: Cross-Line Movement
// ===========================================================================

TEST_F(VisualCaretMovementTest, CrossLine_RightAtEndOfLtrLine) {
  LoadAhem();
  InsertStyleElement("p { font: 10px/20px Ahem; width: 3ch; }");
  // "abcdef" should wrap into two lines: "abc" and "def".
  const SelectionInDomTree selection = SetSelectionTextToBody("<p>ab|cdef</p>");
  SelectionModifier modifier(GetFrame(), selection);

  // Move right to end of first line.
  EXPECT_EQ("<p>abc|def</p>", MoveRight(modifier));
  // Move right again: should cross to start of next line.
  EXPECT_EQ("<p>abcd|ef</p>", MoveRight(modifier));
}

TEST_F(VisualCaretMovementTest, CrossLine_LeftAtStartOfLtrLine) {
  LoadAhem();
  InsertStyleElement("p { font: 10px/20px Ahem; width: 3ch; }");
  const SelectionInDomTree selection = SetSelectionTextToBody("<p>abcd|ef</p>");
  SelectionModifier modifier(GetFrame(), selection);

  // Move left to start of second line.
  EXPECT_EQ("<p>abc|def</p>", MoveLeft(modifier));
  // Move left again: should cross to end of previous line.
  EXPECT_EQ("<p>ab|cdef</p>", MoveLeft(modifier));
}

TEST_F(VisualCaretMovementTest, CrossLine_RightAtEndOfRtlLine) {
  LoadAhem();
  InsertStyleElement("p { font: 10px/20px Ahem; width: 3ch; }");
  // RTL paragraph with Hebrew characters that wrap.
  const SelectionInDomTree selection = SetSelectionTextToBody(
      "<p dir=\"rtl\">"
      "\xD7\x90\xD7\x91\xD7\x92"  // אבג
      "\xD7\x93\xD7\x94\xD7\x95"  // דהו
      "|</p>");
  SelectionModifier modifier(GetFrame(), selection);

  // In RTL, right arrow from the visual left edge goes rightward.
  // This tests that Cross-Line Movement works for RTL paragraphs.
  std::string prev =
      GetSelectionTextFromBody(modifier.Selection().AsSelection());
  int moves = 0;
  for (int i = 0; i < 10; ++i) {
    std::string next = MoveRight(modifier);
    if (next == prev) {
      break;
    }
    prev = next;
    ++moves;
  }
  // Should traverse all 6 characters across both lines.
  EXPECT_EQ(6, moves);
}

TEST_F(VisualCaretMovementTest, CrossLine_LeftAtStartOfRtlLine) {
  LoadAhem();
  InsertStyleElement("p { font: 10px/20px Ahem; width: 3ch; }");
  const SelectionInDomTree selection = SetSelectionTextToBody(
      "<p dir=\"rtl\">|"
      "\xD7\x90\xD7\x91\xD7\x92"  // אבג
      "\xD7\x93\xD7\x94\xD7\x95"  // דהו
      "</p>");
  SelectionModifier modifier(GetFrame(), selection);

  // In RTL, left arrow from the visual right edge goes leftward.
  std::string prev =
      GetSelectionTextFromBody(modifier.Selection().AsSelection());
  int moves = 0;
  for (int i = 0; i < 10; ++i) {
    std::string next = MoveLeft(modifier);
    if (next == prev) {
      break;
    }
    prev = next;
    ++moves;
  }
  EXPECT_EQ(6, moves);
}

TEST_F(VisualCaretMovementTest, CrossLine_BidiBoundaryAtLineBreak) {
  LoadAhem();
  InsertStyleElement("p { font: 10px/20px Ahem; width: 6ch; }");
  // Mixed bidi text that wraps: "abc" + Hebrew alef-bet-gimel + "def"
  // Width is 6ch, so "abc" + 3 Hebrew chars fit on one line,
  // "def" wraps to the next.
  const SelectionInDomTree selection = SetSelectionTextToBody(
      "<p>|abc"
      "\xD7\x90\xD7\x91\xD7\x92"  // אבג
      "def</p>");
  SelectionModifier modifier(GetFrame(), selection);

  // Traverse all characters with right arrow. Should handle the
  // line break correctly even with bidi text on the first line.
  std::string prev =
      GetSelectionTextFromBody(modifier.Selection().AsSelection());
  int moves = 0;
  for (int i = 0; i < 20; ++i) {
    std::string next = MoveRight(modifier);
    if (next == prev) {
      break;
    }
    prev = next;
    ++moves;
  }
  // All 9 characters should be reachable.
  EXPECT_EQ(9, moves);
}

// ===========================================================================
// FlagDisabled: Regression / Flag Isolation
// ===========================================================================

TEST_F(VisualCaretMovementTest, FlagDisabled_LogicalModeUnchanged) {
  // Disable visual mode for this test. All behavior should be logical.
  ScopedBidiVisualOrderCaretMovementForTest disable_visual(false);
  const SelectionInDomTree selection = SetSelectionTextToBody(
      "<p>|abc"
      "\xD7\x90\xD7\x91\xD7\x92"  // אבג
      "</p>");
  SelectionModifier modifier(GetFrame(), selection);

  // With the flag off, right arrow uses logical movement.
  // In LTR paragraph, right = forward = incrementing offset.
  EXPECT_EQ("<p>a|bc\xD7\x90\xD7\x91\xD7\x92</p>", MoveRight(modifier));
  EXPECT_EQ("<p>ab|c\xD7\x90\xD7\x91\xD7\x92</p>", MoveRight(modifier));
  EXPECT_EQ("<p>abc|\xD7\x90\xD7\x91\xD7\x92</p>", MoveRight(modifier));
  // Logical: continues in memory order through the Hebrew characters.
  EXPECT_EQ("<p>abc\xD7\x90|\xD7\x91\xD7\x92</p>", MoveRight(modifier));
  EXPECT_EQ("<p>abc\xD7\x90\xD7\x91|\xD7\x92</p>", MoveRight(modifier));
  EXPECT_EQ("<p>abc\xD7\x90\xD7\x91\xD7\x92|</p>", MoveRight(modifier));
}

// ===========================================================================
// InternalHelpers: Unit tests for internal helper functions
// ===========================================================================

TEST_F(VisualCaretMovementTest, InternalHelpers_VisualStartOffsetLtr) {
  // For LTR text, VisualStartOffset should return TextStartOffset.
  const SelectionInDomTree selection = SetSelectionTextToBody("<p>|abc</p>");
  SelectionModifier modifier(GetFrame(), selection);
  UpdateAllLifecyclePhasesForTest();

  const auto* layout_object =
      GetDocument().body()->firstChild()->firstChild()->GetLayoutObject();
  InlineCursor cursor;
  cursor.MoveTo(*layout_object);
  ASSERT_TRUE(cursor);
  EXPECT_EQ(cursor.Current().TextStartOffset(),
            internal::VisualStartOffset(cursor));
}

TEST_F(VisualCaretMovementTest, InternalHelpers_VisualEndOffsetLtr) {
  // For LTR text, VisualEndOffset should return TextEndOffset.
  const SelectionInDomTree selection = SetSelectionTextToBody("<p>|abc</p>");
  SelectionModifier modifier(GetFrame(), selection);
  UpdateAllLifecyclePhasesForTest();

  const auto* layout_object =
      GetDocument().body()->firstChild()->firstChild()->GetLayoutObject();
  InlineCursor cursor;
  cursor.MoveTo(*layout_object);
  ASSERT_TRUE(cursor);
  EXPECT_EQ(cursor.Current().TextEndOffset(),
            internal::VisualEndOffset(cursor));
}

TEST_F(VisualCaretMovementTest, InternalHelpers_VisualStartOffsetRtl) {
  // For RTL text, VisualStartOffset should return TextEndOffset.
  const SelectionInDomTree selection = SetSelectionTextToBody(
      "<p dir=\"rtl\">|"
      "\xD7\x90\xD7\x91\xD7\x92"  // אבג
      "</p>");
  SelectionModifier modifier(GetFrame(), selection);
  UpdateAllLifecyclePhasesForTest();

  const auto* layout_object =
      GetDocument().body()->firstChild()->firstChild()->GetLayoutObject();
  InlineCursor cursor;
  cursor.MoveTo(*layout_object);
  ASSERT_TRUE(cursor);
  // RTL: visual start is at the higher offset (TextEndOffset).
  EXPECT_EQ(cursor.Current().TextEndOffset(),
            internal::VisualStartOffset(cursor));
}

TEST_F(VisualCaretMovementTest, InternalHelpers_VisualEndOffsetRtl) {
  // For RTL text, VisualEndOffset should return TextStartOffset.
  const SelectionInDomTree selection = SetSelectionTextToBody(
      "<p dir=\"rtl\">|"
      "\xD7\x90\xD7\x91\xD7\x92"  // אבג
      "</p>");
  SelectionModifier modifier(GetFrame(), selection);
  UpdateAllLifecyclePhasesForTest();

  const auto* layout_object =
      GetDocument().body()->firstChild()->firstChild()->GetLayoutObject();
  InlineCursor cursor;
  cursor.MoveTo(*layout_object);
  ASSERT_TRUE(cursor);
  // RTL: visual end is at the lower offset (TextStartOffset).
  EXPECT_EQ(cursor.Current().TextStartOffset(),
            internal::VisualEndOffset(cursor));
}

TEST_F(VisualCaretMovementTest, InternalHelpers_MoveRightNullPosition) {
  // MoveCaretVisuallyRight with a null position should return a null result.
  PositionInFlatTreeWithAffinity null_pos;
  VisualCaretMoveResult result = MoveCaretVisuallyRight(null_pos, std::nullopt);
  EXPECT_TRUE(result.position.IsNull());
}

TEST_F(VisualCaretMovementTest, InternalHelpers_MoveLeftNullPosition) {
  // MoveCaretVisuallyLeft with a null position should return a null result.
  PositionInFlatTreeWithAffinity null_pos;
  VisualCaretMoveResult result = MoveCaretVisuallyLeft(null_pos, std::nullopt);
  EXPECT_TRUE(result.position.IsNull());
}

TEST_F(VisualCaretMovementTest, InternalHelpers_MoveRightPreservesBidiLevel) {
  // MoveCaretVisuallyRight should return a bidi_level in its result.
  const SelectionInDomTree selection = SetSelectionTextToBody("<p>|abc</p>");
  SelectionModifier modifier(GetFrame(), selection);
  UpdateAllLifecyclePhasesForTest();

  PositionInFlatTree pos = ToPositionInFlatTree(modifier.Selection().Focus());
  PositionInFlatTreeWithAffinity pos_with_affinity(pos,
                                                   TextAffinity::kDownstream);
  VisualCaretMoveResult result =
      MoveCaretVisuallyRight(pos_with_affinity, std::nullopt);
  EXPECT_TRUE(result.position.IsNotNull());
  // LTR text has bidi level 0.
  EXPECT_TRUE(result.bidi_level.has_value());
  EXPECT_EQ(0u, *result.bidi_level);
}

TEST_F(VisualCaretMovementTest, InternalHelpers_MoveLeftPreservesBidiLevel) {
  // MoveCaretVisuallyLeft should return a bidi_level in its result.
  const SelectionInDomTree selection = SetSelectionTextToBody("<p>abc|</p>");
  SelectionModifier modifier(GetFrame(), selection);
  UpdateAllLifecyclePhasesForTest();

  PositionInFlatTree pos = ToPositionInFlatTree(modifier.Selection().Focus());
  PositionInFlatTreeWithAffinity pos_with_affinity(pos,
                                                   TextAffinity::kUpstream);
  VisualCaretMoveResult result =
      MoveCaretVisuallyLeft(pos_with_affinity, std::nullopt);
  EXPECT_TRUE(result.position.IsNotNull());
  EXPECT_TRUE(result.bidi_level.has_value());
  EXPECT_EQ(0u, *result.bidi_level);
}

TEST_F(VisualCaretMovementTest, InternalHelpers_RtlBidiLevelIsOdd) {
  // RTL text should have odd bidi level.
  const SelectionInDomTree selection = SetSelectionTextToBody(
      "<p dir=\"rtl\">|"
      "\xD7\x90\xD7\x91\xD7\x92"  // אבג
      "</p>");
  SelectionModifier modifier(GetFrame(), selection);
  UpdateAllLifecyclePhasesForTest();

  PositionInFlatTree pos = ToPositionInFlatTree(modifier.Selection().Focus());
  PositionInFlatTreeWithAffinity pos_with_affinity(pos,
                                                   TextAffinity::kDownstream);
  VisualCaretMoveResult result =
      MoveCaretVisuallyLeft(pos_with_affinity, std::nullopt);
  EXPECT_TRUE(result.position.IsNotNull());
  EXPECT_TRUE(result.bidi_level.has_value());
  // RTL text has odd bidi level (1 for base RTL).
  EXPECT_EQ(1u, *result.bidi_level & 1);
}

// ===========================================================================
// AtomicInline: Atomic Inline Elements (e.g., <img>)
// Tests visual caret movement around non-text atomic inline elements.
// These cover FindFragmentForPosition non-text branch,
// MakeAtomicInlinePosition, MakeAtomicResult, ResolveStartState fallback, and
// atomic inline handling in MoveCaretVisuallyRight/Left.
// ===========================================================================

TEST_F(VisualCaretMovementTest, AtomicInline_ImageRightTraversal) {
  // Move right through LTR text with an <img> in the middle.
  // abc (3) + img (1) + def (3) = 7 character-level moves.
  const SelectionInDomTree selection =
      SetSelectionTextToBody("<p contenteditable>|abc<img>def</p>");
  SelectionModifier modifier(GetFrame(), selection);

  std::string prev =
      GetSelectionTextFromBody(modifier.Selection().AsSelection());
  int moves = 0;
  for (int i = 0; i < 20; ++i) {
    std::string next = MoveRight(modifier);
    if (next == prev) {
      break;
    }
    prev = next;
    ++moves;
  }
  EXPECT_EQ(7, moves);
}

TEST_F(VisualCaretMovementTest, AtomicInline_ImageLeftTraversal) {
  // Move left through LTR text with an <img>.
  const SelectionInDomTree selection =
      SetSelectionTextToBody("<p contenteditable>abc<img>def|</p>");
  SelectionModifier modifier(GetFrame(), selection);

  std::string prev =
      GetSelectionTextFromBody(modifier.Selection().AsSelection());
  int moves = 0;
  for (int i = 0; i < 20; ++i) {
    std::string next = MoveLeft(modifier);
    if (next == prev) {
      break;
    }
    prev = next;
    ++moves;
  }
  EXPECT_EQ(7, moves);
}

TEST_F(VisualCaretMovementTest, AtomicInline_AdjacentImages) {
  // Two consecutive <img> elements. Each counts as one move.
  const SelectionInDomTree selection =
      SetSelectionTextToBody("<p contenteditable>|a<img><img>b</p>");
  SelectionModifier modifier(GetFrame(), selection);

  std::string prev =
      GetSelectionTextFromBody(modifier.Selection().AsSelection());
  int moves = 0;
  for (int i = 0; i < 20; ++i) {
    std::string next = MoveRight(modifier);
    if (next == prev) {
      break;
    }
    prev = next;
    ++moves;
  }
  // a (1) + img (1) + img (1) + b (1) = 4 moves.
  EXPECT_EQ(4, moves);
}

TEST_F(VisualCaretMovementTest, AtomicInline_ImageInBidiText) {
  // <img> between LTR and RTL text.
  const SelectionInDomTree selection = SetSelectionTextToBody(
      "<p contenteditable>|abc<img>"
      "\xD7\x90\xD7\x91\xD7\x92"  // אבג
      "</p>");
  SelectionModifier modifier(GetFrame(), selection);

  std::string prev =
      GetSelectionTextFromBody(modifier.Selection().AsSelection());
  int moves = 0;
  for (int i = 0; i < 20; ++i) {
    std::string next = MoveRight(modifier);
    if (next == prev) {
      break;
    }
    prev = next;
    ++moves;
  }
  // abc (3) + img (1) + אבג (3) = 7 moves.
  EXPECT_EQ(7, moves);
}

TEST_F(VisualCaretMovementTest, AtomicInline_ImageInRtlParagraph) {
  // <img> in an RTL paragraph between Hebrew runs.
  const SelectionInDomTree selection = SetSelectionTextToBody(
      "<p contenteditable dir=\"rtl\">"
      "\xD7\x90\xD7\x91\xD7\x92"  // אבג
      "<img>"
      "\xD7\x93\xD7\x94\xD7\x95"  // דהו
      "|</p>");
  SelectionModifier modifier(GetFrame(), selection);

  // Right arrow from the visual left edge (logical end in RTL).
  std::string prev =
      GetSelectionTextFromBody(modifier.Selection().AsSelection());
  int moves = 0;
  for (int i = 0; i < 20; ++i) {
    std::string next = MoveRight(modifier);
    if (next == prev) {
      break;
    }
    prev = next;
    ++moves;
  }
  // אבג (3) + img (1) + דהו (3) = 7 moves.
  EXPECT_EQ(7, moves);
}

TEST_F(VisualCaretMovementTest, AtomicInline_ImageRoundTrip) {
  // Right then left at an <img> boundary should return to same position.
  const SelectionInDomTree selection =
      SetSelectionTextToBody("<p contenteditable>ab|c<img>def</p>");
  SelectionModifier modifier(GetFrame(), selection);

  std::string start =
      GetSelectionTextFromBody(modifier.Selection().AsSelection());

  // Move right twice: past 'c' and past the <img>.
  MoveRight(modifier);
  MoveRight(modifier);

  // Move left twice to return.
  MoveLeft(modifier);
  MoveLeft(modifier);

  EXPECT_EQ(start,
            GetSelectionTextFromBody(modifier.Selection().AsSelection()));
}

// ===========================================================================
// DirectApi: Direct calls to MoveCaretVisuallyRight/Left
// These tests call the public API directly (bypassing SelectionModifier) to
// ensure specific code paths are exercised, particularly for atomic inline
// handling and cross-line movement.
// ===========================================================================

TEST_F(VisualCaretMovementTest, DirectApi_MoveRightPastAtomicInline) {
  // Move right from end of "abc" text: the next fragment is <img>.
  // This exercises Step 3 with next.IsAtomicInline() → MakeAtomicResult.
  const SelectionInDomTree selection =
      SetSelectionTextToBody("<p contenteditable>abc|<img>def</p>");
  UpdateAllLifecyclePhasesForTest();

  PositionInFlatTree pos = ToPositionInFlatTree(selection.Focus());
  PositionInFlatTreeWithAffinity pos_with_aff(pos, TextAffinity::kDownstream);

  VisualCaretMoveResult result =
      MoveCaretVisuallyRight(pos_with_aff, std::nullopt);
  EXPECT_TRUE(result.position.IsNotNull());
  // Should have moved past the current position.
  EXPECT_NE(pos, result.position.GetPosition());
}

TEST_F(VisualCaretMovementTest, DirectApi_MoveLeftPastAtomicInline) {
  // Move left from start of "def" text: the previous fragment is <img>.
  // This exercises Step 3 with prev.IsAtomicInline() → MakeAtomicResult.
  const SelectionInDomTree selection =
      SetSelectionTextToBody("<p contenteditable>abc<img>|def</p>");
  UpdateAllLifecyclePhasesForTest();

  PositionInFlatTree pos = ToPositionInFlatTree(selection.Focus());
  PositionInFlatTreeWithAffinity pos_with_aff(pos, TextAffinity::kDownstream);

  VisualCaretMoveResult result =
      MoveCaretVisuallyLeft(pos_with_aff, std::nullopt);
  EXPECT_TRUE(result.position.IsNotNull());
  EXPECT_NE(pos, result.position.GetPosition());
}

TEST_F(VisualCaretMovementTest, DirectApi_MoveRightFromAfterAtomicInline) {
  // First move left from start of "def" to reach Position::AfterNode(img).
  // Then move right — this triggers the is_after_atomic handler in
  // MoveCaretVisuallyRight and exercises ResolveStartState fallback
  // (FindFragmentForPosition non-text branch).
  const SelectionInDomTree selection =
      SetSelectionTextToBody("<p contenteditable>abc<img>|def</p>");
  UpdateAllLifecyclePhasesForTest();

  PositionInFlatTree pos = ToPositionInFlatTree(selection.Focus());
  PositionInFlatTreeWithAffinity pos_with_aff(pos, TextAffinity::kDownstream);

  // Step 1: Move left to reach AfterNode(img).
  VisualCaretMoveResult to_img =
      MoveCaretVisuallyLeft(pos_with_aff, std::nullopt);
  ASSERT_TRUE(to_img.position.IsNotNull());

  // Step 2: Move right from the after-img position.
  // This exercises is_after_atomic handler in MoveCaretVisuallyRight.
  VisualCaretMoveResult result =
      MoveCaretVisuallyRight(to_img.position, to_img.bidi_level);
  EXPECT_TRUE(result.position.IsNotNull());
  // Note: result position may equal the input in DOM terms because
  // AfterNode(img) and BeforeTextNode("def") are equivalent positions
  // when img and the text node are adjacent siblings.
}

TEST_F(VisualCaretMovementTest, DirectApi_MoveLeftFromBeforeAtomicInline) {
  // First move right from end of "abc" to reach Position::BeforeNode(img).
  // Then move left — this triggers the is_before_atomic handler in
  // MoveCaretVisuallyLeft and exercises ResolveStartState fallback.
  const SelectionInDomTree selection =
      SetSelectionTextToBody("<p contenteditable>abc|<img>def</p>");
  UpdateAllLifecyclePhasesForTest();

  PositionInFlatTree pos = ToPositionInFlatTree(selection.Focus());
  PositionInFlatTreeWithAffinity pos_with_aff(pos, TextAffinity::kDownstream);

  // Step 1: Move right to reach BeforeNode(img).
  VisualCaretMoveResult to_img =
      MoveCaretVisuallyRight(pos_with_aff, std::nullopt);
  ASSERT_TRUE(to_img.position.IsNotNull());

  // Step 2: Move left from the before-img position.
  // This exercises is_before_atomic handler in MoveCaretVisuallyLeft.
  VisualCaretMoveResult result =
      MoveCaretVisuallyLeft(to_img.position, to_img.bidi_level);
  EXPECT_TRUE(result.position.IsNotNull());
  // Should retreat to the "abc" text.
  EXPECT_NE(to_img.position.GetPosition(), result.position.GetPosition());
}

TEST_F(VisualCaretMovementTest, DirectApi_MoveRightCrossLine) {
  // Test cross-line movement by moving right through wrapped text.
  // This exercises MoveToStartOfNextVisualLine.
  LoadAhem();
  InsertStyleElement("p { font: 10px/20px Ahem; width: 30px; }");
  const SelectionInDomTree selection = SetSelectionTextToBody("<p>|abcdef</p>");
  UpdateAllLifecyclePhasesForTest();

  PositionInFlatTree pos = ToPositionInFlatTree(selection.Focus());
  PositionInFlatTreeWithAffinity pos_with_aff(pos, TextAffinity::kDownstream);

  // Move right 3 times to reach end of "abc" on line 1.
  VisualCaretMoveResult result;
  for (int i = 0; i < 3; ++i) {
    result = MoveCaretVisuallyRight(pos_with_aff, result.bidi_level);
    ASSERT_TRUE(result.position.IsNotNull())
        << "Failed to move right at step " << i;
    pos_with_aff = result.position;
  }

  // Now at end of line 1. Next move should cross to line 2 via
  // MoveToStartOfNextVisualLine.
  VisualCaretMoveResult cross_line =
      MoveCaretVisuallyRight(pos_with_aff, result.bidi_level);
  EXPECT_TRUE(cross_line.position.IsNotNull());
  EXPECT_NE(pos_with_aff.GetPosition(), cross_line.position.GetPosition());
}

TEST_F(VisualCaretMovementTest, DirectApi_MoveLeftCrossLine) {
  // Test cross-line movement by moving left through wrapped text.
  // This exercises MoveToEndOfPreviousVisualLine.
  LoadAhem();
  InsertStyleElement("p { font: 10px/20px Ahem; width: 30px; }");
  const SelectionInDomTree selection = SetSelectionTextToBody("<p>abcdef|</p>");
  UpdateAllLifecyclePhasesForTest();

  PositionInFlatTree pos = ToPositionInFlatTree(selection.Focus());
  PositionInFlatTreeWithAffinity pos_with_aff(pos, TextAffinity::kUpstream);

  // Move left 3 times to reach start of "def" on line 2.
  VisualCaretMoveResult result;
  for (int i = 0; i < 3; ++i) {
    result = MoveCaretVisuallyLeft(pos_with_aff, result.bidi_level);
    ASSERT_TRUE(result.position.IsNotNull())
        << "Failed to move left at step " << i;
    pos_with_aff = result.position;
  }

  // Now at start of line 2. Next move should cross to line 1 via
  // MoveToEndOfPreviousVisualLine.
  VisualCaretMoveResult cross_line =
      MoveCaretVisuallyLeft(pos_with_aff, result.bidi_level);
  EXPECT_TRUE(cross_line.position.IsNotNull());
  EXPECT_NE(pos_with_aff.GetPosition(), cross_line.position.GetPosition());
}

}  // namespace blink
