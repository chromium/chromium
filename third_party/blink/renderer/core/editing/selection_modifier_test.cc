// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/selection_modifier.h"

#include "third_party/blink/renderer/core/editing/editing_behavior.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"

namespace blink {

class SelectionModifierTest : public EditingTestBase {
 protected:
  std::string MoveBackwardByLine(SelectionModifier& modifier) {
    modifier.Modify(SelectionModifyAlteration::kMove,
                    SelectionModifyDirection::kBackward,
                    TextGranularity::kLine);
    return GetSelectionTextFromBody(modifier.Selection().AsSelection());
  }

  std::string MoveForwardByLine(SelectionModifier& modifier) {
    modifier.Modify(SelectionModifyAlteration::kMove,
                    SelectionModifyDirection::kForward, TextGranularity::kLine);
    return GetSelectionTextFromBody(modifier.Selection().AsSelection());
  }
};

TEST_F(SelectionModifierTest, ExtendForwardByWordNone) {
  SetBodyContent("abc");
  SelectionModifier modifier(GetFrame(), SelectionInDOMTree());
  modifier.Modify(SelectionModifyAlteration::kExtend,
                  SelectionModifyDirection::kForward, TextGranularity::kWord);
  // We should not crash here. See http://crbug.com/832061
  EXPECT_EQ(SelectionInDOMTree(), modifier.Selection().AsSelection());
}

TEST_F(SelectionModifierTest, MoveForwardByWordNone) {
  SetBodyContent("abc");
  SelectionModifier modifier(GetFrame(), SelectionInDOMTree());
  modifier.Modify(SelectionModifyAlteration::kMove,
                  SelectionModifyDirection::kForward, TextGranularity::kWord);
  // We should not crash here. See http://crbug.com/832061
  EXPECT_EQ(SelectionInDOMTree(), modifier.Selection().AsSelection());
}

// http://crbug.com/1300781
TEST_F(SelectionModifierTest, MoveByLineBlockInInline) {
  LoadAhem();
  InsertStyleElement(
      "div {"
      "font: 10px/20px Ahem;"
      "padding: 10px;"
      "writing-mode: horizontal-tb;"
      "}"
      "b { background: orange; }");
  const SelectionInDOMTree selection =
      SetSelectionTextToBody("<div>ab|c<b><p>ABC</p><p>DEF</p>def</b></div>");
  SelectionModifier modifier(GetFrame(), selection);

  EXPECT_EQ("<div>abc<b><p>AB|C</p><p>DEF</p>def</b></div>",
            MoveForwardByLine(modifier));
  EXPECT_EQ("<div>abc<b><p>ABC</p><p>DE|F</p>def</b></div>",
            MoveForwardByLine(modifier));
  EXPECT_EQ("<div>abc<b><p>ABC</p><p>DEF</p>de|f</b></div>",
            MoveForwardByLine(modifier));

  EXPECT_EQ("<div>abc<b><p>ABC</p><p>DE|F</p>def</b></div>",
            MoveBackwardByLine(modifier));
  EXPECT_EQ("<div>abc<b><p>AB|C</p><p>DEF</p>def</b></div>",
            MoveBackwardByLine(modifier));
  EXPECT_EQ("<div>ab|c<b><p>ABC</p><p>DEF</p>def</b></div>",
            MoveBackwardByLine(modifier));
}

TEST_F(SelectionModifierTest, MoveByLineHorizontal) {
  LoadAhem();
  InsertStyleElement(
      "p {"
      "font: 10px/20px Ahem;"
      "padding: 10px;"
      "writing-mode: horizontal-tb;"
      "}");
  const SelectionInDOMTree selection =
      SetSelectionTextToBody("<p>ab|c<br>d<br><br>ghi</p>");
  SelectionModifier modifier(GetFrame(), selection);

  EXPECT_EQ("<p>abc<br>d|<br><br>ghi</p>", MoveForwardByLine(modifier));
  EXPECT_EQ("<p>abc<br>d<br>|<br>ghi</p>", MoveForwardByLine(modifier));
  EXPECT_EQ("<p>abc<br>d<br><br>gh|i</p>", MoveForwardByLine(modifier));

  EXPECT_EQ("<p>abc<br>d<br>|<br>ghi</p>", MoveBackwardByLine(modifier));
  EXPECT_EQ("<p>abc<br>d|<br><br>ghi</p>", MoveBackwardByLine(modifier));
  EXPECT_EQ("<p>ab|c<br>d<br><br>ghi</p>", MoveBackwardByLine(modifier));
}

TEST_F(SelectionModifierTest, MoveByLineMultiColumnSingleText) {
  LoadAhem();
  InsertStyleElement(
      "div { font: 10px/15px Ahem; column-count: 3; width: 20ch; }");
  const SelectionInDOMTree selection =
      SetSelectionTextToBody("<div>|abc def ghi jkl mno pqr</div>");
  // This HTML is rendered as:
  //    abc ghi mno
  //    def jkl pqr
  SelectionModifier modifier(GetFrame(), selection);

  EXPECT_EQ("<div>abc |def ghi jkl mno pqr</div>", MoveForwardByLine(modifier));
  EXPECT_EQ("<div>abc def |ghi jkl mno pqr</div>", MoveForwardByLine(modifier));
  EXPECT_EQ("<div>abc def ghi |jkl mno pqr</div>", MoveForwardByLine(modifier));
  EXPECT_EQ("<div>abc def ghi jkl |mno pqr</div>", MoveForwardByLine(modifier));
  EXPECT_EQ("<div>abc def ghi jkl mno |pqr</div>", MoveForwardByLine(modifier));
  EXPECT_EQ("<div>abc def ghi jkl mno pqr|</div>", MoveForwardByLine(modifier));

  EXPECT_EQ("<div>abc def ghi jkl |mno pqr</div>",
            MoveBackwardByLine(modifier));
  EXPECT_EQ("<div>abc def ghi |jkl mno pqr</div>",
            MoveBackwardByLine(modifier));
  EXPECT_EQ("<div>abc def |ghi jkl mno pqr</div>",
            MoveBackwardByLine(modifier));
  EXPECT_EQ("<div>abc |def ghi jkl mno pqr</div>",
            MoveBackwardByLine(modifier));
  EXPECT_EQ("<div>|abc def ghi jkl mno pqr</div>",
            MoveBackwardByLine(modifier));
}

TEST_F(SelectionModifierTest, MoveByLineVertical) {
  LoadAhem();
  InsertStyleElement(
      "p {"
      "font: 10px/20px Ahem;"
      "padding: 10px;"
      "writing-mode: vertical-rl;"
      "}");
  const SelectionInDOMTree selection =
      SetSelectionTextToBody("<p>ab|c<br>d<br><br>ghi</p>");
  SelectionModifier modifier(GetFrame(), selection);

  EXPECT_EQ("<p>abc<br>d|<br><br>ghi</p>", MoveForwardByLine(modifier));
  EXPECT_EQ("<p>abc<br>d<br>|<br>ghi</p>", MoveForwardByLine(modifier));
  EXPECT_EQ("<p>abc<br>d<br><br>gh|i</p>", MoveForwardByLine(modifier));

  EXPECT_EQ("<p>abc<br>d<br>|<br>ghi</p>", MoveBackwardByLine(modifier));
  EXPECT_EQ("<p>abc<br>d|<br><br>ghi</p>", MoveBackwardByLine(modifier));
  EXPECT_EQ("<p>ab|c<br>d<br><br>ghi</p>", MoveBackwardByLine(modifier));
}

TEST_F(SelectionModifierTest, PreviousLineWithDisplayNone) {
  InsertStyleElement("body{font-family: monospace}");
  const SelectionInDOMTree selection = SetSelectionTextToBody(
      "<div contenteditable>"
      "<div>foo bar</div>"
      "<div>foo <b style=\"display:none\">qux</b> bar baz|</div>"
      "</div>");
  SelectionModifier modifier(GetFrame(), selection);
  modifier.Modify(SelectionModifyAlteration::kMove,
                  SelectionModifyDirection::kBackward, TextGranularity::kLine);
  EXPECT_EQ(
      "<div contenteditable>"
      "<div>foo bar|</div>"
      "<div>foo <b style=\"display:none\">qux</b> bar baz</div>"
      "</div>",
      GetSelectionTextFromBody(modifier.Selection().AsSelection()));
}

// For http://crbug.com/1104582
TEST_F(SelectionModifierTest, PreviousSentenceWithNull) {
  InsertStyleElement("b {display:inline-block}");
  const SelectionInDOMTree selection =
      SetSelectionTextToBody("<b><b><a>|</a></b></b>");
  SelectionModifier modifier(GetFrame(), selection);
  // We call |PreviousSentence()| with null-position.
  EXPECT_FALSE(modifier.Modify(SelectionModifyAlteration::kMove,
                               SelectionModifyDirection::kBackward,
                               TextGranularity::kSentence));
}

// For http://crbug.com/1100971
TEST_F(SelectionModifierTest, StartOfSentenceWithNull) {
  InsertStyleElement("b {display:inline-block}");
  const SelectionInDOMTree selection =
      SetSelectionTextToBody("|<b><b><a></a></b></b>");
  SelectionModifier modifier(GetFrame(), selection);
  // We call |StartOfSentence()| with null-position.
  EXPECT_FALSE(modifier.Modify(SelectionModifyAlteration::kMove,
                               SelectionModifyDirection::kBackward,
                               TextGranularity::kSentenceBoundary));
}

TEST_F(SelectionModifierTest, MoveCaretWithShadow) {
  const char* body_content =
      "a a"
      "<div id='host'>"
      "<span slot='e'>e e</span>"
      "<span slot='c'>c c</span>"
      "</div>"
      "f f";
  const char* shadow_content =
      "b b"
      "<slot name='c'></slot>"
      "d d"
      "<slot name='e'></slot>";
  LoadAhem();
  InsertStyleElement("body {font-family: Ahem}");
  SetBodyContent(body_content);
  Element* host = GetDocument().getElementById(AtomicString("host"));
  ShadowRoot& shadow_root =
      host->AttachShadowRootForTesting(ShadowRootMode::kOpen);
  shadow_root.setInnerHTML(shadow_content);
  UpdateAllLifecyclePhasesForTest();

  Element* body = GetDocument().body();
  Node* a = body->childNodes()->item(0);
  Node* b = shadow_root.childNodes()->item(0);
  Node* c = host->QuerySelector(AtomicString("[slot=c]"))->firstChild();
  Node* d = shadow_root.childNodes()->item(2);
  Node* e = host->QuerySelector(AtomicString("[slot=e]"))->firstChild();
  Node* f = body->childNodes()->item(2);

  auto makeSelection = [&](Position position) {
    return SelectionInDOMTree::Builder().Collapse(position).Build();
  };
  SelectionModifyAlteration move = SelectionModifyAlteration::kMove;
  SelectionModifyDirection direction;
  TextGranularity granularity;

  {
    // Test moving forward, character by character.
    direction = SelectionModifyDirection::kForward;
    granularity = TextGranularity::kCharacter;
    SelectionModifier modifier(GetFrame(), makeSelection(Position(body, 0)));
    EXPECT_EQ(Position(a, 0), modifier.Selection().Anchor());
    for (Node* node : {a, b, c, d, e, f}) {
      if (node == b || node == f) {
        modifier.Modify(move, direction, granularity);
        EXPECT_EQ(node == b ? Position::BeforeNode(*node) : Position(node, 0),
                  modifier.Selection().Anchor());
      }
      modifier.Modify(move, direction, granularity);
      EXPECT_EQ(Position(node, 1), modifier.Selection().Anchor());
      modifier.Modify(move, direction, granularity);
      EXPECT_EQ(Position(node, 2), modifier.Selection().Anchor());
      modifier.Modify(move, direction, granularity);
      EXPECT_EQ(Position(node, 3), modifier.Selection().Anchor());
    }
  }
  {
    // Test moving backward, character by character.
    direction = SelectionModifyDirection::kBackward;
    granularity = TextGranularity::kCharacter;
    SelectionModifier modifier(GetFrame(), makeSelection(Position(body, 3)));
    for (Node* node : {f, e, d, c, b, a}) {
      EXPECT_EQ(Position(node, 3), modifier.Selection().Anchor());
      modifier.Modify(move, direction, granularity);
      EXPECT_EQ(Position(node, 2), modifier.Selection().Anchor());
      modifier.Modify(move, direction, granularity);
      EXPECT_EQ(Position(node, 1), modifier.Selection().Anchor());
      modifier.Modify(move, direction, granularity);
      if (node == f || node == b) {
        EXPECT_EQ(node == b ? Position::BeforeNode(*node) : Position(node, 0),
                  modifier.Selection().Anchor());
        modifier.Modify(move, direction, granularity);
      }
    }
    EXPECT_EQ(Position(a, 0), modifier.Selection().Anchor());
  }
  {
    // Test moving forward, word by word.
    direction = SelectionModifyDirection::kForward;
    granularity = TextGranularity::kWord;
    bool skip_space =
        GetFrame().GetEditor().Behavior().ShouldSkipSpaceWhenMovingRight();
    SelectionModifier modifier(GetFrame(), makeSelection(Position(body, 0)));
    EXPECT_EQ(Position(a, 0), modifier.Selection().Anchor());
    for (Node* node : {a, b, c, d, e, f}) {
      if (node == b || node == f) {
        modifier.Modify(move, direction, granularity);
        EXPECT_EQ(node == b ? Position::BeforeNode(*node) : Position(node, 0),
                  modifier.Selection().Anchor());
      }
      modifier.Modify(move, direction, granularity);
      EXPECT_EQ(Position(node, skip_space ? 2 : 1),
                modifier.Selection().Anchor());
      if (node == a || node == e || node == f) {
        modifier.Modify(move, direction, granularity);
        EXPECT_EQ(Position(node, 3), modifier.Selection().Anchor());
      }
    }
  }
  {
    // Test moving backward, word by word.
    direction = SelectionModifyDirection::kBackward;
    granularity = TextGranularity::kWord;
    SelectionModifier modifier(GetFrame(), makeSelection(Position(body, 3)));
    for (Node* node : {f, e, d, c, b, a}) {
      if (node == f || node == e || node == a) {
        EXPECT_EQ(Position(node, 3), modifier.Selection().Anchor());
        modifier.Modify(move, direction, granularity);
      }
      EXPECT_EQ(Position(node, 2), modifier.Selection().Anchor());
      modifier.Modify(move, direction, granularity);
      if (node == f || node == b) {
        EXPECT_EQ(node == b ? Position::BeforeNode(*node) : Position(node, 0),
                  modifier.Selection().Anchor());
        modifier.Modify(move, direction, granularity);
      }
    }
    EXPECT_EQ(Position(a, 0), modifier.Selection().Anchor());
  }

  // Place the contents into different lines
  InsertStyleElement("span {display: block}");
  UpdateAllLifecyclePhasesForTest();

  {
    // Test moving forward, line by line.
    direction = SelectionModifyDirection::kForward;
    granularity = TextGranularity::kLine;
    for (int i = 0; i <= 3; ++i) {
      SelectionModifier modifier(GetFrame(), makeSelection(Position(a, i)));
      for (Node* node : {a, b, c, d, e, f}) {
        EXPECT_EQ(i == 0 && node == b ? Position::BeforeNode(*node)
                                      : Position(node, i),
                  modifier.Selection().Anchor());
        modifier.Modify(move, direction, granularity);
      }
      EXPECT_EQ(Position(f, 3), modifier.Selection().Anchor());
    }
  }
  {
    // Test moving backward, line by line.
    direction = SelectionModifyDirection::kBackward;
    granularity = TextGranularity::kLine;
    for (int i = 0; i <= 3; ++i) {
      SelectionModifier modifier(GetFrame(), makeSelection(Position(f, i)));
      for (Node* node : {f, e, d, c, b, a}) {
        EXPECT_EQ(i == 0 && node == b ? Position::BeforeNode(*node)
                                      : Position(node, i),
                  modifier.Selection().Anchor());
        modifier.Modify(move, direction, granularity);
      }
      EXPECT_EQ(Position(a, 0), modifier.Selection().Anchor());
    }
  }
}

// For https://crbug.com/1155342 and https://crbug.com/1155309
TEST_F(SelectionModifierTest, PreviousParagraphOfObject) {
  const SelectionInDOMTree selection =
      SetSelectionTextToBody("<object>|</object>");
  SelectionModifier modifier(GetFrame(), selection);
  modifier.Modify(SelectionModifyAlteration::kMove,
                  SelectionModifyDirection::kBackward,
                  TextGranularity::kParagraph);
  EXPECT_EQ("|<object></object>",
            GetSelectionTextFromBody(modifier.Selection().AsSelection()));
}

// For https://crbug.com/1177295
TEST_F(SelectionModifierTest, PositionDisconnectedInFlatTree1) {
  const SelectionInDOMTree selection = SetSelectionTextToBody(
      "<div id=a><div id=b><div id=c>^x|</div></div></div>");
  SetShadowContent("", "a");
  SetShadowContent("", "b");
  SetShadowContent("", "c");
  SelectionModifier modifier(GetFrame(), selection);
  modifier.Modify(SelectionModifyAlteration::kMove,
                  SelectionModifyDirection::kBackward,
                  TextGranularity::kParagraph);
  EXPECT_EQ("<div id=\"a\"><div id=\"b\"><div id=\"c\">x</div></div></div>",
            GetSelectionTextFromBody(modifier.Selection().AsSelection()));
}

// For https://crbug.com/1177295
TEST_F(SelectionModifierTest, PositionDisconnectedInFlatTree2) {
  SetBodyContent("<div id=host>x</div>y");
  SetShadowContent("", "host");
  Element* host = GetElementById("host");
  Node* text = host->firstChild();
  Position positions[] = {
      Position::BeforeNode(*host),         Position::FirstPositionInNode(*host),
      Position::LastPositionInNode(*host), Position::AfterNode(*host),
      Position::BeforeNode(*text),         Position::FirstPositionInNode(*text),
      Position::LastPositionInNode(*text), Position::AfterNode(*text)};
  for (const Position& anchor : positions) {
    EXPECT_TRUE(anchor.IsConnected());
    bool flat_anchor_is_connected = ToPositionInFlatTree(anchor).IsConnected();
    EXPECT_EQ(anchor.AnchorNode() == host, flat_anchor_is_connected);
    for (const Position& focus : positions) {
      const SelectionInDOMTree& selection =
          SelectionInDOMTree::Builder().SetBaseAndExtent(anchor, focus).Build();
      Selection().SetSelection(selection, SetSelectionOptions());
      SelectionModifier modifier(GetFrame(), selection);
      modifier.Modify(SelectionModifyAlteration::kExtend,
                      SelectionModifyDirection::kForward,
                      TextGranularity::kParagraph);
      EXPECT_TRUE(focus.IsConnected());
      bool flat_focus_is_connected =
          ToPositionInFlatTree(selection.Focus()).IsConnected();
      EXPECT_EQ(flat_anchor_is_connected || flat_focus_is_connected
                    ? "<div id=\"host\">x</div>^y|"
                    : "<div id=\"host\">x</div>y",
                GetSelectionTextFromBody(modifier.Selection().AsSelection()));
    }
  }
}

// For https://crbug.com/1312704
TEST_F(SelectionModifierTest, OptgroupAndTable) {
  InsertStyleElement(
      "optgroup, table { display: inline-table; }"
      "table { appearance:button; }");
  SelectionModifier modifier(
      GetFrame(), SetSelectionTextToBody(
                      "<optgroup>^</optgroup>|<table><td></td></table>"));
  EXPECT_TRUE(modifier.Modify(SelectionModifyAlteration::kExtend,
                              SelectionModifyDirection::kForward,
                              TextGranularity::kLine));

  const SelectionInDOMTree& selection = modifier.Selection().AsSelection();
  EXPECT_EQ(
      "<optgroup></optgroup><table><tbody><tr><td></td></tr></tbody></table>",
      GetSelectionTextFromBody(selection));

  Element* optgroup = GetDocument().QuerySelector(AtomicString("optgroup"));
  ShadowRoot* shadow_root = optgroup->GetShadowRoot();
  Element* label =
      shadow_root->getElementById(shadow_element_names::kIdOptGroupLabel);
  EXPECT_EQ(Position(label, 0), selection.Anchor());
  EXPECT_EQ(Position(shadow_root, 1), selection.Focus());
}

TEST_F(SelectionModifierTest, EditableVideo) {
  const SelectionInDOMTree selection =
      SetSelectionTextToBody("a^<video contenteditable> </video>|");
  GetFrame().GetSettings()->SetEditingBehaviorType(
      mojom::EditingBehavior::kEditingUnixBehavior);
  for (SelectionModifyDirection direction :
       {SelectionModifyDirection::kBackward, SelectionModifyDirection::kForward,
        SelectionModifyDirection::kLeft, SelectionModifyDirection::kRight}) {
    for (TextGranularity granularity :
         {TextGranularity::kCharacter, TextGranularity::kWord,
          TextGranularity::kSentence, TextGranularity::kLine,
          TextGranularity::kParagraph, TextGranularity::kSentenceBoundary,
          TextGranularity::kLineBoundary, TextGranularity::kParagraphBoundary,
          TextGranularity::kDocumentBoundary}) {
      SelectionModifier modifier(GetFrame(), selection);
      // We should not crash here. See http://crbug.com/1376218
      modifier.Modify(SelectionModifyAlteration::kMove, direction, granularity);
      EXPECT_EQ("a|<video contenteditable> </video>",
                GetSelectionTextFromBody(modifier.Selection().AsSelection()))
          << "Direction " << (int)direction << ", granularity "
          << (int)granularity;
      ;
    }
  }
}

}  // namespace blink
