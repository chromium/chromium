// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_ios_uiview.h"

#include "testing/platform_test.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"

@interface RenderWidgetUIView (Testing)
- (BOOL)shouldInsertCharacter:(const blink::WebKeyboardEvent&)webKeyboardEvent;
- (std::string)moveSelectionCommand:(UITextLayoutDirection)direction;
- (std::string)extendSelectionCommand:(UITextLayoutDirection)direction;
- (std::vector<std::string>)
    moveSelectionCommands:(UITextStorageDirection)direction
            byGranularity:(UITextGranularity)granularity;
- (std::vector<std::string>)
    extendSelectionCommands:(UITextStorageDirection)direction
              byGranularity:(UITextGranularity)granularity;
- (std::vector<std::string>)
    deleteSelectionCommands:(UITextStorageDirection)direction
              toGranularity:(UITextGranularity)granularity;
@end

namespace content {

class RenderWidgetHostViewIOSUIViewTest : public PlatformTest {
 public:
  void SetUp() override {
    uiview_ = [[RenderWidgetUIView alloc] initWithWidget:nil];
  }

 protected:
  RenderWidgetUIView* uiview_;
};

blink::WebKeyboardEvent CreateKeyboardEvent(char16_t character,
                                            int modifiers = 0) {
  blink::WebKeyboardEvent event;
  event.text[0] = character;
  event.text[1] = 0;  // Null-terminate the string
  event.SetModifiers(modifiers);
  return event;
}

TEST_F(RenderWidgetHostViewIOSUIViewTest, ShouldInsertCharacter) {
  struct {
    std::string category;
    std::vector<char16_t> characters;
    bool expected_result;
  } test_cases[] = {
      {"Basic Latin", {u'a', u'Z', u'0', u'9', u'@', u' '}, true},
      {"Control Characters", {0x00, 0x01, 0x1F}, false},
      {"Latin-1 Supplement", {0x00A1, 0x00FF}, true},  // ¡, ÿ
      {"Currency Symbols", {0x20AC, 0x00A5}, true},    // €, ¥
      {"Mathematical Symbols", {0x2200, 0x22FF}, true},
  };

  for (const auto& test_case : test_cases) {
    for (char16_t c : test_case.characters) {
      EXPECT_EQ([uiview_ shouldInsertCharacter:CreateKeyboardEvent(c)],
                test_case.expected_result);
    }
  }
}

TEST_F(RenderWidgetHostViewIOSUIViewTest, ShouldInsertMultiCharacterInput) {
  blink::WebKeyboardEvent emoji_event;

  // Set up a multi-character emoji sequence
  const std::vector<char16_t> emoji = {0xD83D, 0xDE00,
                                       0};  // 😀 (surrogate pair)
  int i = 0;
  for (char16_t e : emoji) {
    if (e != 0) {
      emoji_event.text[i++] = e;
    }
  }

  EXPECT_TRUE([uiview_ shouldInsertCharacter:emoji_event]);
}

TEST_F(RenderWidgetHostViewIOSUIViewTest, ShouldNotInsertWithControlModifiers) {
  // Control key should prevent character insertion for ASCII characters
  EXPECT_FALSE([uiview_
      shouldInsertCharacter:CreateKeyboardEvent(
                                u'a', blink::WebInputEvent::kControlKey)]);

  // Meta key should also prevent character insertion for ASCII characters
  EXPECT_FALSE([uiview_
      shouldInsertCharacter:CreateKeyboardEvent(
                                u'b', blink::WebInputEvent::kMetaKey)]);

  // Other modifiers should not prevent insertion
  EXPECT_TRUE([uiview_
      shouldInsertCharacter:CreateKeyboardEvent(
                                u'c', blink::WebInputEvent::kShiftKey)]);
  EXPECT_TRUE(
      [uiview_ shouldInsertCharacter:CreateKeyboardEvent(
                                         u'd', blink::WebInputEvent::kAltKey)]);
}

TEST_F(RenderWidgetHostViewIOSUIViewTest, MoveSelectionCommand) {
  // Create a map of directions to expected commands
  struct {
    UITextLayoutDirection direction;
    std::string expected_command;
  } test_cases[] = {
      {UITextLayoutDirectionLeft, "moveLeft"},
      {UITextLayoutDirectionRight, "moveRight"},
      {UITextLayoutDirectionUp, "moveUp"},
      {UITextLayoutDirectionDown, "moveDown"},
  };

  // Verify each case
  for (const auto& test_case : test_cases) {
    std::string command = [uiview_ moveSelectionCommand:test_case.direction];
    EXPECT_EQ(command, test_case.expected_command);
  }
}

TEST_F(RenderWidgetHostViewIOSUIViewTest, ExtendSelectionCommand) {
  // Create a map of directions to expected commands
  struct {
    UITextLayoutDirection direction;
    std::string expected_command;
  } test_cases[] = {
      {UITextLayoutDirectionLeft, "moveLeftAndModifySelection"},
      {UITextLayoutDirectionRight, "moveRightAndModifySelection"},
      {UITextLayoutDirectionUp, "moveUpAndModifySelection"},
      {UITextLayoutDirectionDown, "moveDownAndModifySelection"},
  };

  // Verify each case
  for (const auto& test_case : test_cases) {
    std::string command = [uiview_ extendSelectionCommand:test_case.direction];
    EXPECT_EQ(command, test_case.expected_command);
  }
}

TEST_F(RenderWidgetHostViewIOSUIViewTest, MoveSelectionCommands) {
  // Define all test cases
  struct {
    UITextStorageDirection direction;
    UITextGranularity granularity;
    std::vector<std::string> expected_commands;
  } test_cases[] = {
      // Character granularity
      {UITextStorageDirectionForward,
       UITextGranularityCharacter,
       {"moveForward"}},
      {UITextStorageDirectionBackward,
       UITextGranularityCharacter,
       {"moveBackward"}},

      // Word granularity
      {UITextStorageDirectionForward,
       UITextGranularityWord,
       {"moveWordForward"}},
      {UITextStorageDirectionBackward,
       UITextGranularityWord,
       {"moveWordBackward"}},

      // Sentence granularity
      {UITextStorageDirectionForward,
       UITextGranularitySentence,
       {"moveToEndOfSentence"}},
      {UITextStorageDirectionBackward,
       UITextGranularitySentence,
       {"moveToBeginningOfSentence"}},

      // Paragraph granularity
      {UITextStorageDirectionForward,
       UITextGranularityParagraph,
       {"moveForward", "moveToEndOfParagraph"}},
      {UITextStorageDirectionBackward,
       UITextGranularityParagraph,
       {"moveBackward", "moveToBeginningOfParagraph"}},

      // Line granularity
      {UITextStorageDirectionForward,
       UITextGranularityLine,
       {"moveToEndOfLine"}},
      {UITextStorageDirectionBackward,
       UITextGranularityLine,
       {"moveToBeginningOfLine"}},

      // Document granularity
      {UITextStorageDirectionForward,
       UITextGranularityDocument,
       {"moveToEndOfDocument"}},
      {UITextStorageDirectionBackward,
       UITextGranularityDocument,
       {"moveToBeginningOfDocument"}},
  };

  // Verify each case
  for (const auto& test_case : test_cases) {
    std::vector<std::string> commands =
        [uiview_ moveSelectionCommands:test_case.direction
                         byGranularity:test_case.granularity];

    ASSERT_EQ(commands.size(), test_case.expected_commands.size());

    for (size_t i = 0; i < commands.size(); i++) {
      EXPECT_EQ(commands[i], test_case.expected_commands[i]);
    }
  }
}

TEST_F(RenderWidgetHostViewIOSUIViewTest, ExtendSelectionCommands) {
  // Define all test cases
  struct {
    UITextStorageDirection direction;
    UITextGranularity granularity;
    std::vector<std::string> expected_commands;
  } test_cases[] = {
      // Character granularity
      {UITextStorageDirectionForward,
       UITextGranularityCharacter,
       {"moveBackwardAndModifySelection"}},
      {UITextStorageDirectionBackward,
       UITextGranularityCharacter,
       {"moveForwardAndModifySelection"}},

      // Word granularity
      {UITextStorageDirectionForward,
       UITextGranularityWord,
       {"moveWordForwardAndModifySelection"}},
      {UITextStorageDirectionBackward,
       UITextGranularityWord,
       {"moveWordBackwardAndModifySelection"}},

      // Sentence granularity
      {UITextStorageDirectionForward,
       UITextGranularitySentence,
       {"moveToEndOfSentenceAndModifySelection"}},
      {UITextStorageDirectionBackward,
       UITextGranularitySentence,
       {"moveToBeginningOfSentenceAndModifySelection"}},

      // Paragraph granularity
      {UITextStorageDirectionForward,
       UITextGranularityParagraph,
       {"moveForwardAndModifySelection",
        "moveToEndOfParagraphAndModifySelection"}},
      {UITextStorageDirectionBackward,
       UITextGranularityParagraph,
       {"moveBackwardAndModifySelection",
        "moveToBeginningOfParagraphAndModifySelection"}},

      // Line granularity
      {UITextStorageDirectionForward,
       UITextGranularityLine,
       {"moveToEndOfLineAndModifySelection"}},
      {UITextStorageDirectionBackward,
       UITextGranularityLine,
       {"moveToBeginningOfLineAndModifySelection"}},

      // Document granularity
      {UITextStorageDirectionForward,
       UITextGranularityDocument,
       {"moveToEndOfDocumentAndModifySelection"}},
      {UITextStorageDirectionBackward,
       UITextGranularityDocument,
       {"moveToBeginningOfDocumentAndModifySelection"}},
  };

  // Verify each case
  for (const auto& test_case : test_cases) {
    std::vector<std::string> commands =
        [uiview_ extendSelectionCommands:test_case.direction
                           byGranularity:test_case.granularity];

    ASSERT_EQ(commands.size(), test_case.expected_commands.size());

    for (size_t i = 0; i < commands.size(); i++) {
      EXPECT_EQ(commands[i], test_case.expected_commands[i]);
    }
  }
}

TEST_F(RenderWidgetHostViewIOSUIViewTest, DeleteSelectionCommands) {
  // Define all test cases
  struct {
    UITextStorageDirection direction;
    UITextGranularity granularity;
    std::vector<std::string> expected_commands;
  } test_cases[] = {
      // Character granularity
      {UITextStorageDirectionForward,
       UITextGranularityCharacter,
       {"deleteForward"}},
      {UITextStorageDirectionBackward,
       UITextGranularityCharacter,
       {"deleteBackward"}},

      // Word granularity
      {UITextStorageDirectionForward,
       UITextGranularityWord,
       {"deleteWordForward"}},
      {UITextStorageDirectionBackward,
       UITextGranularityWord,
       {"deleteWordBackward"}},

      // Sentence granularity
      {UITextStorageDirectionForward,
       UITextGranularitySentence,
       {"moveToEndOfSentenceAndModifySelection", "deleteBackward"}},
      {UITextStorageDirectionBackward,
       UITextGranularitySentence,
       {"moveToBeginningOfSentenceAndModifySelection", "deleteBackward"}},

      // Paragraph granularity
      {UITextStorageDirectionForward,
       UITextGranularityParagraph,
       {"deleteToEndOfParagraph"}},
      {UITextStorageDirectionBackward,
       UITextGranularityParagraph,
       {"deleteToBeginningOfParagraph"}},

      // Line granularity
      {UITextStorageDirectionForward,
       UITextGranularityLine,
       {"deleteToEndOfLine"}},
      {UITextStorageDirectionBackward,
       UITextGranularityLine,
       {"deleteToBeginningOfLine"}},

      // Document granularity (default case)
      {UITextStorageDirectionForward,
       UITextGranularityDocument,
       {"moveToEndOfDocumentAndModifySelection", "deleteBackward"}},
      {UITextStorageDirectionBackward,
       UITextGranularityDocument,
       {"moveToBeginningOfDocumentAndModifySelection", "deleteBackward"}},
  };

  // Verify each case
  for (const auto& test_case : test_cases) {
    std::vector<std::string> commands =
        [uiview_ deleteSelectionCommands:test_case.direction
                           toGranularity:test_case.granularity];

    ASSERT_EQ(commands.size(), test_case.expected_commands.size());

    for (size_t i = 0; i < commands.size(); i++) {
      EXPECT_EQ(commands[i], test_case.expected_commands[i]);
    }
  }
}

}  // namespace content
