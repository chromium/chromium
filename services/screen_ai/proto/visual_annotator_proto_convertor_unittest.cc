// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/screen_ai/proto/visual_annotator_proto_convertor.h"

#include <string>

#include "services/screen_ai/proto/chrome_screen_ai.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/gfx/geometry/rect.h"

namespace {

chrome_screen_ai::Rect CreateProtoRect(const gfx::Rect& source, float angle) {
  chrome_screen_ai::Rect result;
  result.set_x(source.x());
  result.set_y(source.y());
  result.set_width(source.width());
  result.set_height(source.height());
  result.set_angle(angle);
  return result;
}

void InitSymbolBox(chrome_screen_ai::SymbolBox* symbol_box,
                   const gfx::Rect& bounding_box,
                   const char* text) {
  *symbol_box->mutable_bounding_box() =
      CreateProtoRect(bounding_box, /*angle=*/0);
  symbol_box->set_utf8_string(text);
}

void InitWordBox(chrome_screen_ai::WordBox* word_box,
                 const gfx::Rect& bounding_box,
                 const char* text,
                 const char* language,
                 chrome_screen_ai::Direction direction,
                 const gfx::Rect& whitespace_bounding_box,
                 int32_t background_rgb_value,
                 int32_t foreground_rgb_value,
                 float angle) {
  *word_box->mutable_bounding_box() = CreateProtoRect(bounding_box, angle);
  word_box->set_utf8_string(text);
  word_box->set_language(language);
  word_box->set_direction(direction);
  *word_box->mutable_whitespace_bounding_box() =
      CreateProtoRect(whitespace_bounding_box, /*angle=*/0);
  word_box->set_estimate_color_success(true);
  word_box->set_background_rgb_value(background_rgb_value);
  word_box->set_foreground_rgb_value(foreground_rgb_value);
}

void InitLineBox(chrome_screen_ai::LineBox* line_box,
                 const gfx::Rect& bounding_box,
                 const char* text,
                 const char* language,
                 chrome_screen_ai::Direction direction,
                 int32_t block_id,
                 int32_t paragraph_id,
                 float angle,
                 bool add_word = false) {
  *line_box->mutable_bounding_box() = CreateProtoRect(bounding_box, angle);
  line_box->set_utf8_string(text);
  line_box->set_language(language);
  line_box->set_direction(direction);
  line_box->set_block_id(block_id);
  line_box->set_paragraph_id(paragraph_id);
  if (add_word) {
    InitWordBox(line_box->add_words(), bounding_box, text, language, direction,
                /*whitespace_bounding_box=*/gfx::Rect(),
                /*background_rgb_value=*/0,
                /*foreground_rgb_value=*/0,
                /*angle=*/0);
  }
}

}  // namespace

namespace screen_ai {

using ScreenAIVisualAnnotatorProtoConvertorTest = testing::Test;

TEST_F(ScreenAIVisualAnnotatorProtoConvertorTest, SimpleResults) {
  chrome_screen_ai::VisualAnnotation annotation;
  {
    chrome_screen_ai::LineBox* line_0 = annotation.add_lines();

    InitWordBox(line_0->add_words(),
                /*bounding_box=*/gfx::Rect(100, 100, 250, 20),
                /*text=*/"Hello",
                /*language=*/"en",
                /*direction=*/chrome_screen_ai::DIRECTION_LEFT_TO_RIGHT,
                /*whitespace_bounding_box=*/
                gfx::Rect(110, 100, 10, 10),
                /*background_rgb_value=*/0xffffff00,
                /*foreground_rgb_value=*/0x00000000,  // Black on white.
                /*angle=*/1);

    InitWordBox(line_0->add_words(),
                /*bounding_box=*/gfx::Rect(350, 100, 250, 20),
                /*text=*/"world",
                /*language=*/"en",
                /*direction=*/chrome_screen_ai::DIRECTION_LEFT_TO_RIGHT,
                /*whitespace_bounding_box=*/
                gfx::Rect(),
                /*background_rgb_value=*/0xffffff00,
                /*foreground_rgb_value=*/0xff000000,  // Blue on white.
                /*angle=*/2);

    InitLineBox(line_0,
                /*bounding_box=*/gfx::Rect(100, 100, 500, 20),
                /*text=*/"Hello world",
                /*language=*/"en",
                /*direction=*/chrome_screen_ai::DIRECTION_LEFT_TO_RIGHT,
                /*block_id=*/0,
                /*paragraph_id=*/0,
                /*angle=*/1.5);
  }

  // Testing conversion to Mojo.
  {
    mojom::VisualAnnotationPtr annot =
        ConvertProtoToVisualAnnotation(annotation);
    EXPECT_EQ(annot->lines.size(), static_cast<unsigned long>(1));
    mojom::LineBoxPtr& line = annot->lines[0];
    EXPECT_EQ(line->bounding_box.x(), 100);
    EXPECT_EQ(line->bounding_box.y(), 100);
    EXPECT_EQ(line->bounding_box.width(), 500);
    EXPECT_EQ(line->bounding_box.height(), 20);
    EXPECT_EQ(line->bounding_box_angle, 1.5);
    EXPECT_EQ(line->text_line, "Hello world");
    EXPECT_EQ(line->block_id, 0);
    EXPECT_EQ(line->paragraph_id, 0);
    EXPECT_EQ(line->words.size(), static_cast<unsigned long>(2));

    mojom::WordBoxPtr& word_0 = line->words[0];
    EXPECT_EQ(word_0->word, "Hello");
    EXPECT_EQ(word_0->language, "en");
    EXPECT_EQ(word_0->bounding_box.x(), 100);
    EXPECT_EQ(word_0->bounding_box.y(), 100);
    EXPECT_EQ(word_0->bounding_box.width(), 250);
    EXPECT_EQ(word_0->bounding_box.height(), 20);
    EXPECT_EQ(word_0->bounding_box_angle, 1);
    EXPECT_NE(word_0->whitespace_bounding_box.width(), 0);
    EXPECT_NE(word_0->whitespace_bounding_box.height(), 0);
    EXPECT_EQ(word_0->direction, mojom::Direction::DIRECTION_LEFT_TO_RIGHT);

    mojom::WordBoxPtr& word_1 = line->words[1];
    EXPECT_EQ(word_1->word, "world");
    EXPECT_EQ(word_1->language, "en");
    EXPECT_EQ(word_1->bounding_box.x(), 350);
    EXPECT_EQ(word_1->bounding_box.y(), 100);
    EXPECT_EQ(word_1->bounding_box.width(), 250);
    EXPECT_EQ(word_1->bounding_box.height(), 20);
    EXPECT_EQ(word_1->bounding_box_angle, 2);
    EXPECT_EQ(word_1->whitespace_bounding_box.width(), 0);
    EXPECT_EQ(word_1->whitespace_bounding_box.height(), 0);
    EXPECT_EQ(word_1->direction, mojom::Direction::DIRECTION_LEFT_TO_RIGHT);
  }

// Testing conversion to AxTreeUpdate which is only used on ChromeOS.
#if BUILDFLAG(IS_CHROMEOS)
  {
    gfx::Rect snapshot_bounds(800, 900);
    screen_ai::ResetNodeIDForTesting();
    const ui::AXTreeUpdate update =
        VisualAnnotationToAXTreeUpdate(annotation, snapshot_bounds);

    const std::string expected_update(
        "AXTreeUpdate: root id -2\n"
        "id=-2 region class_name=ocred_page child_ids=-3,-5,-8 (0, 0)-(800, "
        "900) is_page_breaking_object=true\n"
        "  id=-3 banner child_ids=-4 (0, 0)-(1, 1)\n"
        "    id=-4 staticText name=Start of extracted text (0, 0)-(1, 1)\n"
        "  id=-5 paragraph child_ids=-6 (99, 100)-(500, 33)\n"
        "    id=-6 staticText name=Hello world child_ids=-7 (99, 100)-(500, "
        "33) text_direction=ltr language=en\n"
        "      id=-7 inlineTextBox name=Hello world (99, 100)-(500, 28) "
        "background_color=&FFFFFF00 color=&0 text_direction=ltr "
        "word_starts=0,6 word_ends=5,10\n"
        "  id=-8 contentInfo child_ids=-9 (800, 900)-(1, 1)\n"
        "    id=-9 staticText name=End of extracted text (800, 900)-(1, 1)\n");
    EXPECT_EQ(expected_update, update.ToString());
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
}

TEST_F(ScreenAIVisualAnnotatorProtoConvertorTest, MultipleLanguages) {
  chrome_screen_ai::VisualAnnotation annotation;
  {
    chrome_screen_ai::LineBox* line_0 = annotation.add_lines();

    InitWordBox(line_0->add_words(),
                /*bounding_box=*/gfx::Rect(100, 100, 250, 20),
                /*text=*/"Bonjour",
                /*language=*/"fr",
                /*direction=*/chrome_screen_ai::DIRECTION_LEFT_TO_RIGHT,
                /*whitespace_bounding_box=*/gfx::Rect(110, 10, 10, 10),
                /*background_rgb_value=*/0xffffff00,
                /*foreground_rgb_value=*/0x00000000,  // Black on white.
                /*angle=*/0);

    InitWordBox(line_0->add_words(),
                /*bounding_box=*/gfx::Rect(350, 100, 250, 20),
                /*text=*/"world",
                /*language=*/"en",
                /*direction=*/chrome_screen_ai::DIRECTION_LEFT_TO_RIGHT,
                /*whitespace_bounding_box=*/gfx::Rect(),
                /*background_rgb_value=*/0xffffff00,
                /*foreground_rgb_value=*/0xff000000,  // Blue on white.
                /*angle=*/0);

    InitLineBox(line_0,
                /*bounding_box=*/gfx::Rect(100, 100, 500, 20),
                /*text=*/"Bonjour world",
                /*language=*/"en",
                /*direction=*/chrome_screen_ai::DIRECTION_LEFT_TO_RIGHT,
                /*block_id=*/0,
                /*paragraph_id=*/0,
                /*angle=*/0);
  }

  // Testing conversion to Mojo.
  {
    mojom::VisualAnnotationPtr annot =
        ConvertProtoToVisualAnnotation(annotation);
    EXPECT_EQ(annot->lines.size(), static_cast<unsigned long>(1));
    mojom::LineBoxPtr& line = annot->lines[0];
    EXPECT_EQ(line->bounding_box.x(), 100);
    EXPECT_EQ(line->bounding_box.y(), 100);
    EXPECT_EQ(line->bounding_box.width(), 500);
    EXPECT_EQ(line->bounding_box.height(), 20);
    EXPECT_EQ(line->bounding_box_angle, 0);
    EXPECT_EQ(line->text_line, "Bonjour world");
    EXPECT_EQ(line->language, "en");
    EXPECT_EQ(line->block_id, 0);
    EXPECT_EQ(line->paragraph_id, 0);
    EXPECT_EQ(line->words.size(), static_cast<unsigned long>(2));

    mojom::WordBoxPtr& word_0 = line->words[0];
    EXPECT_EQ(word_0->bounding_box.x(), 100);
    EXPECT_EQ(word_0->bounding_box.y(), 100);
    EXPECT_EQ(word_0->bounding_box.width(), 250);
    EXPECT_EQ(word_0->bounding_box.height(), 20);
    EXPECT_EQ(word_0->bounding_box_angle, 0);
    EXPECT_EQ(word_0->word, "Bonjour");
    EXPECT_EQ(word_0->language, "fr");
    EXPECT_NE(word_0->whitespace_bounding_box.width(), 0);
    EXPECT_NE(word_0->whitespace_bounding_box.height(), 0);
    EXPECT_EQ(word_0->direction, mojom::Direction::DIRECTION_LEFT_TO_RIGHT);

    mojom::WordBoxPtr& word_1 = line->words[1];
    EXPECT_EQ(word_1->bounding_box.x(), 350);
    EXPECT_EQ(word_1->bounding_box.y(), 100);
    EXPECT_EQ(word_1->bounding_box.width(), 250);
    EXPECT_EQ(word_1->bounding_box.height(), 20);
    EXPECT_EQ(word_1->bounding_box_angle, 0);
    EXPECT_EQ(word_1->word, "world");
    EXPECT_EQ(word_1->language, "en");
    EXPECT_EQ(word_1->whitespace_bounding_box.width(), 0);
    EXPECT_EQ(word_1->whitespace_bounding_box.height(), 0);
    EXPECT_EQ(word_1->direction, mojom::Direction::DIRECTION_LEFT_TO_RIGHT);
  }

// Testing conversion to AxTreeUpdate.
#if BUILDFLAG(IS_CHROMEOS)
  {
    gfx::Rect snapshot_bounds(800, 900);
    screen_ai::ResetNodeIDForTesting();
    const ui::AXTreeUpdate update =
        VisualAnnotationToAXTreeUpdate(annotation, snapshot_bounds);

    const std::string expected_update(
        "AXTreeUpdate: root id -2\n"
        "id=-2 region class_name=ocred_page child_ids=-3,-5,-9 (0, 0)-(800, "
        "900) is_page_breaking_object=true\n"
        "  id=-3 banner child_ids=-4 (0, 0)-(1, 1)\n"
        "    id=-4 staticText name=Start of extracted text (0, 0)-(1, 1)\n"
        "  id=-5 paragraph child_ids=-6 (100, 100)-(500, 20)\n"
        "    id=-6 staticText name=Bonjour world child_ids=-7,-8 "
        "(100, 100)-(500, 20) text_direction=ltr language=en\n"
        "      id=-7 inlineTextBox name=Bonjour  (100, 100)-(250, 20) "
        "background_color=&FFFFFF00 color=&0 text_direction=ltr language=fr "
        "word_starts=0 word_ends=7\n"
        "      id=-8 inlineTextBox name=world (350, 100)-(250, 20) "
        "background_color=&FFFFFF00 color=&FF000000 text_direction=ltr "
        "word_starts=0 word_ends=4\n"
        "  id=-9 contentInfo child_ids=-10 (800, 900)-(1, 1)\n"
        "    id=-10 staticText name=End of extracted text (800, 900)-(1, 1)\n");
    EXPECT_EQ(expected_update, update.ToString());
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
}

TEST_F(ScreenAIVisualAnnotatorProtoConvertorTest,
       VisualAnnotationToAXTreeUpdate_OcrResults_RightToLeftMultiByte) {
  chrome_screen_ai::VisualAnnotation annotation;
  {
    chrome_screen_ai::LineBox* line_0 = annotation.add_lines();
    chrome_screen_ai::WordBox* word_0 = line_0->add_words();
    chrome_screen_ai::WordBox* word_1 = line_0->add_words();

    InitSymbolBox(word_0->add_symbols(),
                  /*bounding_box=*/gfx::Rect(133, 100, 4, 20),
                  /*text=*/"ر");

    InitSymbolBox(word_0->add_symbols(),
                  /*bounding_box=*/gfx::Rect(129, 100, 4, 20),
                  /*text=*/"و");

    InitSymbolBox(word_0->add_symbols(),
                  /*bounding_box=*/gfx::Rect(125, 100, 4, 20),
                  /*text=*/"ز");

    InitWordBox(word_0,
                /*bounding_box=*/gfx::Rect(125, 100, 12, 20),
                /*text=*/"روز",
                /*language=*/"fa",
                /*direction=*/chrome_screen_ai::DIRECTION_RIGHT_TO_LEFT,
                /*whitespace_bounding_box=*/gfx::Rect(135, 100, 10, 10),
                /*background_rgb_value=*/0xffffff00,
                /*foreground_rgb_value=*/0xff000000,  // Blue on white.
                /*angle=*/0);

    InitSymbolBox(word_1->add_symbols(),
                  /*bounding_box=*/gfx::Rect(114, 100, 6, 20),
                  /*text=*/"خ");

    InitSymbolBox(word_1->add_symbols(),
                  /*bounding_box=*/gfx::Rect(110, 4, 50, 20),
                  /*text=*/"و");

    InitSymbolBox(word_1->add_symbols(),
                  /*bounding_box=*/gfx::Rect(100, 100, 10, 20),
                  /*text=*/"ش");

    InitWordBox(word_1,
                /*bounding_box=*/gfx::Rect(100, 100, 20, 20),
                /*text=*/"خوش",
                /*language=*/"fa",
                /*direction=*/chrome_screen_ai::DIRECTION_RIGHT_TO_LEFT,
                /*whitespace_bounding_box=*/gfx::Rect(),
                /*background_rgb_value=*/0xffffff00,
                /*foreground_rgb_value=*/0x00000000,  // Black on white.
                /*angle=*/0);

    InitLineBox(line_0,
                /*bounding_box=*/gfx::Rect(100, 100, 37, 20),
                /*text=*/"روز خوش",
                /*language=*/"fa",
                /*direction=*/chrome_screen_ai::DIRECTION_RIGHT_TO_LEFT,
                /*block_id=*/0,
                /*paragraph_id=*/0,
                /*angle=*/0);
  }

  // Testing conversion to Mojo.
  {
    mojom::VisualAnnotationPtr annot =
        ConvertProtoToVisualAnnotation(annotation);
    EXPECT_EQ(annot->lines.size(), static_cast<unsigned long>(1));
    mojom::LineBoxPtr& line = annot->lines[0];
    EXPECT_EQ(line->bounding_box.x(), 100);
    EXPECT_EQ(line->bounding_box.y(), 100);
    EXPECT_EQ(line->bounding_box.width(), 37);
    EXPECT_EQ(line->bounding_box.height(), 20);
    EXPECT_EQ(line->bounding_box_angle, 0);
    EXPECT_EQ(line->text_line, "روز خوش");
    EXPECT_EQ(line->language, "fa");
    EXPECT_EQ(line->block_id, 0);
    EXPECT_EQ(line->paragraph_id, 0);
    EXPECT_EQ(line->words.size(), static_cast<unsigned long>(2));

    mojom::WordBoxPtr& word_0 = line->words[0];
    EXPECT_EQ(word_0->bounding_box.x(), 125);
    EXPECT_EQ(word_0->bounding_box.y(), 100);
    EXPECT_EQ(word_0->bounding_box.width(), 12);
    EXPECT_EQ(word_0->bounding_box.height(), 20);
    EXPECT_EQ(word_0->bounding_box_angle, 0);
    EXPECT_EQ(word_0->word, "روز");
    EXPECT_EQ(word_0->language, "fa");
    EXPECT_NE(word_0->whitespace_bounding_box.width(), 0);
    EXPECT_NE(word_0->whitespace_bounding_box.height(), 0);
    EXPECT_EQ(word_0->direction, mojom::Direction::DIRECTION_RIGHT_TO_LEFT);

    mojom::WordBoxPtr& word_1 = line->words[1];
    EXPECT_EQ(word_1->bounding_box.x(), 100);
    EXPECT_EQ(word_1->bounding_box.y(), 100);
    EXPECT_EQ(word_1->bounding_box.width(), 20);
    EXPECT_EQ(word_1->bounding_box.height(), 20);
    EXPECT_EQ(word_1->bounding_box_angle, 0);
    EXPECT_EQ(word_1->word, "خوش");
    EXPECT_EQ(word_1->language, "fa");
    EXPECT_EQ(word_1->whitespace_bounding_box.width(), 0);
    EXPECT_EQ(word_1->whitespace_bounding_box.height(), 0);
    EXPECT_EQ(word_1->direction, mojom::Direction::DIRECTION_RIGHT_TO_LEFT);
  }

// Testing conversion to AxTreeUpdate.
#if BUILDFLAG(IS_CHROMEOS)
  {
    gfx::Rect snapshot_bounds(800, 900);
    screen_ai::ResetNodeIDForTesting();
    const ui::AXTreeUpdate update =
        VisualAnnotationToAXTreeUpdate(annotation, snapshot_bounds);

    const std::string expected_update(
        "AXTreeUpdate: root id -2\n"
        "id=-2 region class_name=ocred_page child_ids=-3,-5,-8 (0, 0)-(800, "
        "900) is_page_breaking_object=true\n"
        "  id=-3 banner child_ids=-4 (0, 0)-(1, 1)\n"
        "    id=-4 staticText name=Start of extracted text (0, 0)-(1, 1)\n"
        "  id=-5 paragraph child_ids=-6 (100, 100)-(37, 20)\n"
        "    id=-6 staticText name=روز خوش child_ids=-7 (100, 100)-(37, 20) "
        "text_direction=rtl language=fa\n"
        "      id=-7 inlineTextBox name=روز خوش (100, 100)-(37, 20) "
        "background_color=&FFFFFF00 color=&FF000000 text_direction=rtl "
        "character_offsets=4,8,12,17,23,27,37 word_starts=0,4 word_ends=3,6\n"
        "  id=-8 contentInfo child_ids=-9 (800, 900)-(1, 1)\n"
        "    id=-9 staticText name=End of extracted text (800, 900)-(1, 1)\n");
    EXPECT_EQ(expected_update, update.ToString());
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
}

// Character offsets are only returned in AxTreeUpdate.
#if BUILDFLAG(IS_CHROMEOS)
TEST_F(ScreenAIVisualAnnotatorProtoConvertorTest,
       CharacterOffsets_LeftToRight_NotRotated) {
  chrome_screen_ai::VisualAnnotation annotation;
  {
    chrome_screen_ai::LineBox* line_0 = annotation.add_lines();
    chrome_screen_ai::WordBox* word_0 = line_0->add_words();
    chrome_screen_ai::WordBox* word_1 = line_0->add_words();

    InitSymbolBox(word_0->add_symbols(),
                  /*bounding_box=*/gfx::Rect(100, 100, 4, 19),
                  /*text=*/"D");

    InitSymbolBox(word_0->add_symbols(),
                  /*bounding_box=*/gfx::Rect(104, 100, 6, 19),
                  /*text=*/"a");

    InitSymbolBox(word_0->add_symbols(),
                  /*bounding_box=*/gfx::Rect(113, 100, 6, 19),
                  /*text=*/"y");

    InitWordBox(word_0,
                /*bounding_box=*/gfx::Rect(100, 100, 22, 19),
                /*text=*/"Day",
                /*language=*/"en",
                /*direction=*/chrome_screen_ai::DIRECTION_LEFT_TO_RIGHT,
                /*whitespace_bounding_box=*/
                gfx::Rect(110, 100, 10, 10),
                /*background_rgb_value=*/0xffffff00,
                /*foreground_rgb_value=*/0xff000000,  // Blue on white.
                /*angle=*/0);

    InitSymbolBox(word_1->add_symbols(),
                  /*bounding_box=*/gfx::Rect(122, 100, 4, 19),
                  /*text=*/"O");

    InitSymbolBox(word_1->add_symbols(),
                  /*bounding_box=*/gfx::Rect(126, 100, 6, 19),
                  /*text=*/"n");

    InitSymbolBox(word_1->add_symbols(),
                  /*bounding_box=*/gfx::Rect(135, 100, 6, 19),
                  /*text=*/"e");

    InitWordBox(word_1,
                /*bounding_box=*/gfx::Rect(122, 100, 19, 19),
                /*text=*/"One",
                /*language=*/"en",
                /*direction=*/chrome_screen_ai::DIRECTION_LEFT_TO_RIGHT,
                /*whitespace_bounding_box=*/gfx::Rect(),
                /*background_rgb_value=*/0xffffff00,
                /*foreground_rgb_value=*/0xff000000,  // Blue on white.
                /*angle=*/0);

    InitLineBox(line_0,
                /*bounding_box=*/gfx::Rect(100, 100, 41, 19),
                /*text=*/"Day One",
                /*language=*/"en",
                /*direction=*/chrome_screen_ai::DIRECTION_LEFT_TO_RIGHT,
                /*block_id=*/0,
                /*paragraph_id=*/0,
                /*angle=*/0);
  }

  {
    gfx::Rect snapshot_bounds(800, 900);
    screen_ai::ResetNodeIDForTesting();
    const ui::AXTreeUpdate update =
        VisualAnnotationToAXTreeUpdate(annotation, snapshot_bounds);

    const std::string expected_update(
        "AXTreeUpdate: root id -2\n"
        "id=-2 region class_name=ocred_page child_ids=-3,-5,-8 (0, 0)-(800, "
        "900) is_page_breaking_object=true\n"
        "  id=-3 banner child_ids=-4 (0, 0)-(1, 1)\n"
        "    id=-4 staticText name=Start of extracted text (0, 0)-(1, 1)\n"
        "  id=-5 paragraph child_ids=-6 (100, 100)-(41, 19)\n"
        "    id=-6 staticText name=Day One child_ids=-7 (100, 100)-(41, 19) "
        "text_direction=ltr language=en\n"
        "      id=-7 inlineTextBox name=Day One (100, 100)-(41, 19) "
        "background_color=&FFFFFF00 color=&FF000000 text_direction=ltr "
        "character_offsets=4,10,19,22,26,32,41 word_starts=0,4 word_ends=3,6\n"
        "  id=-8 contentInfo child_ids=-9 (800, 900)-(1, 1)\n"
        "    id=-9 staticText name=End of extracted text (800, 900)-(1, 1)\n");
    EXPECT_EQ(expected_update, update.ToString());
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS)

// Character offsets are only returned in AxTreeUpdate.
#if BUILDFLAG(IS_CHROMEOS)
TEST_F(ScreenAIVisualAnnotatorProtoConvertorTest,
       CharacterOffsets_LeftToRight_Rotated) {
  chrome_screen_ai::VisualAnnotation annotation;
  {
    chrome_screen_ai::LineBox* line_0 = annotation.add_lines();
    chrome_screen_ai::WordBox* word_0 = line_0->add_words();
    chrome_screen_ai::WordBox* word_1 = line_0->add_words();

    InitSymbolBox(word_0->add_symbols(),
                  /*bounding_box=*/gfx::Rect(100, 100, 19, 4),
                  /*text=*/"D");

    InitSymbolBox(word_0->add_symbols(),
                  /*bounding_box=*/gfx::Rect(100, 104, 19, 6),
                  /*text=*/"a");

    InitSymbolBox(word_0->add_symbols(),
                  /*bounding_box=*/gfx::Rect(100, 113, 19, 6),
                  /*text=*/"y");

    InitWordBox(word_0,
                /*bounding_box=*/gfx::Rect(100, 100, 19, 22),
                /*text=*/"Day",
                /*language=*/"en",
                /*direction=*/chrome_screen_ai::DIRECTION_LEFT_TO_RIGHT,
                /*whitespace_bounding_box=*/gfx::Rect(110, 100, 10, 10),
                /*background_rgb_value=*/0xffffff00,
                /*foreground_rgb_value=*/0xff000000,  // Blue on white.
                /*angle=*/90);

    InitSymbolBox(word_1->add_symbols(),
                  /*bounding_box=*/gfx::Rect(100, 122, 19, 4),
                  /*text=*/"O");

    InitSymbolBox(word_1->add_symbols(),
                  /*bounding_box=*/gfx::Rect(100, 126, 19, 6),
                  /*text=*/"n");

    InitSymbolBox(word_1->add_symbols(),
                  /*bounding_box=*/gfx::Rect(100, 135, 19, 6),
                  /*text=*/"e");

    InitWordBox(word_1,
                /*bounding_box=*/gfx::Rect(100, 122, 19, 19),
                /*text=*/"One",
                /*language=*/"en",
                /*direction=*/chrome_screen_ai::DIRECTION_LEFT_TO_RIGHT,
                /*whitespace_bounding_box=*/gfx::Rect(),
                /*background_rgb_value=*/0xffffff00,
                /*foreground_rgb_value=*/0xff000000,  // Blue on white.
                /*angle=*/90);

    InitLineBox(line_0,
                /*bounding_box=*/gfx::Rect(100, 100, 19, 41),
                /*text=*/"Day One",
                /*language=*/"en",
                /*direction=*/chrome_screen_ai::DIRECTION_LEFT_TO_RIGHT,
                /*block_id=*/0,
                /*paragraph_id=*/0,
                /*angle=*/90);
  }

  {
    gfx::Rect snapshot_bounds(800, 900);
    screen_ai::ResetNodeIDForTesting();
    const ui::AXTreeUpdate update =
        VisualAnnotationToAXTreeUpdate(annotation, snapshot_bounds);

    const std::string expected_update(
        "AXTreeUpdate: root id -2\n"
        "id=-2 region class_name=ocred_page child_ids=-3,-5,-8 (0, 0)-(800, "
        "900) is_page_breaking_object=true\n"
        "  id=-3 banner child_ids=-4 (0, 0)-(1, 1)\n"
        "    id=-4 staticText name=Start of extracted text (0, 0)-(1, 1)\n"
        "  id=-5 paragraph child_ids=-6 (59, 100)-(41, 19)\n"
        "    id=-6 staticText name=Day One child_ids=-7 (59, 100)-(41, 19) "
        "text_direction=ltr language=en\n"
        "      id=-7 inlineTextBox name=Day One (78, 100)-(22, 41) "
        "background_color=&FFFFFF00 color=&FF000000 text_direction=ltr "
        "character_offsets=4,10,19,22,26,32,41 word_starts=0,4 word_ends=3,6\n"
        "  id=-8 contentInfo child_ids=-9 (800, 900)-(1, 1)\n"
        "    id=-9 staticText name=End of extracted text (800, 900)-(1, 1)\n");
    EXPECT_EQ(expected_update, update.ToString());
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS)

TEST_F(ScreenAIVisualAnnotatorProtoConvertorTest, Paragraphs) {
  chrome_screen_ai::VisualAnnotation annotation;
  {
    typedef struct {
      int block_id;
      int paragraph_id;
      const char* text;
    } LineInfo;

    // Expected paragraphs: (Jan, Feb, Mar), (Apr, May), (Jun)
    LineInfo lines[] = {{0, 0, "Jan"}, {0, 0, "Feb"}, {0, 0, "Mar"},
                        {0, 1, "Apr"}, {0, 1, "May"}, {1, 0, "Jun"}};

    int y = 100;
    for (auto& line : lines) {
      InitLineBox(annotation.add_lines(),
                  /*bounding_box=*/gfx::Rect(100, y, 100, 20),
                  /*text=*/line.text,
                  /*language=*/"en",
                  /*direction=*/chrome_screen_ai::DIRECTION_LEFT_TO_RIGHT,
                  /*block_id=*/line.block_id,
                  /*paragraph_id=*/line.paragraph_id,
                  /*angle=*/0,
                  /*add_word=*/true);
      y += 20;
    }
  }

  // Testing conversion to Mojo.
  {
    mojom::VisualAnnotationPtr annot =
        ConvertProtoToVisualAnnotation(annotation);
    EXPECT_EQ(annot->lines.size(), static_cast<unsigned long>(6));
    EXPECT_EQ(annot->lines[0]->text_line, "Jan");
    EXPECT_EQ(annot->lines[0]->block_id, 0);
    EXPECT_EQ(annot->lines[0]->paragraph_id, 0);

    EXPECT_EQ(annot->lines[1]->text_line, "Feb");
    EXPECT_EQ(annot->lines[1]->block_id, 0);
    EXPECT_EQ(annot->lines[1]->paragraph_id, 0);

    EXPECT_EQ(annot->lines[2]->text_line, "Mar");
    EXPECT_EQ(annot->lines[2]->block_id, 0);
    EXPECT_EQ(annot->lines[2]->paragraph_id, 0);

    EXPECT_EQ(annot->lines[3]->text_line, "Apr");
    EXPECT_EQ(annot->lines[3]->block_id, 0);
    EXPECT_EQ(annot->lines[3]->paragraph_id, 1);

    EXPECT_EQ(annot->lines[4]->text_line, "May");
    EXPECT_EQ(annot->lines[4]->block_id, 0);
    EXPECT_EQ(annot->lines[4]->paragraph_id, 1);

    EXPECT_EQ(annot->lines[5]->text_line, "Jun");
    EXPECT_EQ(annot->lines[5]->block_id, 1);
    EXPECT_EQ(annot->lines[5]->paragraph_id, 0);
  }

// Testing conversion to AxTreeUpdate.
#if BUILDFLAG(IS_CHROMEOS)
  {
    gfx::Rect snapshot_bounds(800, 900);
    screen_ai::ResetNodeIDForTesting();
    const ui::AXTreeUpdate update =
        VisualAnnotationToAXTreeUpdate(annotation, snapshot_bounds);

    const std::string expected_update(
        "AXTreeUpdate: root id -2\n"
        "id=-2 region class_name=ocred_page child_ids=-3,-5,-12,-17,-20 (0, "
        "0)-(800, 900) is_page_breaking_object=true\n"
        "  id=-3 banner child_ids=-4 (0, 0)-(1, 1)\n"
        "    id=-4 staticText name=Start of extracted text (0, 0)-(1, 1)\n"
        "  id=-5 paragraph child_ids=-6,-8,-10 (100, 100)-(100, 60)\n"
        "    id=-6 staticText name=Jan child_ids=-7 (100, 100)-(100, 20) "
        "text_direction=ltr language=en\n"
        "      id=-7 inlineTextBox name=Jan (100, 100)-(100, 20) "
        "background_color=&0 color=&0 text_direction=ltr word_starts=0 "
        "word_ends=2\n"
        "    id=-8 staticText name=Feb child_ids=-9 (100, 120)-(100, 20) "
        "text_direction=ltr language=en\n"
        "      id=-9 inlineTextBox name=Feb (100, 120)-(100, 20) "
        "background_color=&0 color=&0 text_direction=ltr word_starts=0 "
        "word_ends=2\n"
        "    id=-10 staticText name=Mar child_ids=-11 (100, 140)-(100, 20) "
        "text_direction=ltr language=en\n"
        "      id=-11 inlineTextBox name=Mar (100, 140)-(100, 20) "
        "background_color=&0 color=&0 text_direction=ltr word_starts=0 "
        "word_ends=2\n"
        "  id=-12 paragraph child_ids=-13,-15 (100, 160)-(100, 40)\n"
        "    id=-13 staticText name=Apr child_ids=-14 (100, 160)-(100, 20) "
        "text_direction=ltr language=en\n"
        "      id=-14 inlineTextBox name=Apr (100, 160)-(100, 20) "
        "background_color=&0 color=&0 text_direction=ltr word_starts=0 "
        "word_ends=2\n"
        "    id=-15 staticText name=May child_ids=-16 (100, 180)-(100, 20) "
        "text_direction=ltr language=en\n"
        "      id=-16 inlineTextBox name=May (100, 180)-(100, 20) "
        "background_color=&0 color=&0 text_direction=ltr word_starts=0 "
        "word_ends=2\n"
        "  id=-17 paragraph child_ids=-18 (100, 200)-(100, 20)\n"
        "    id=-18 staticText name=Jun child_ids=-19 (100, 200)-(100, 20) "
        "text_direction=ltr language=en\n"
        "      id=-19 inlineTextBox name=Jun (100, 200)-(100, 20) "
        "background_color=&0 color=&0 text_direction=ltr word_starts=0 "
        "word_ends=2\n"
        "  id=-20 contentInfo child_ids=-21 (800, 900)-(1, 1)\n"
        "    id=-21 staticText name=End of extracted text (800, 900)-(1, 1)\n");
    EXPECT_EQ(expected_update, update.ToString());
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
}

}  // namespace screen_ai
