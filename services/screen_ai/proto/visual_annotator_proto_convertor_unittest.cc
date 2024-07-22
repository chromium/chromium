// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/screen_ai/proto/visual_annotator_proto_convertor.h"

#include <string>

#include "services/screen_ai/proto/chrome_screen_ai.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_tree_update.h"

namespace {

void InitLineBox(chrome_screen_ai::LineBox* line_box,
                 int32_t x,
                 int32_t y,
                 int32_t width,
                 int32_t height,
                 const char* text,
                 const char* language,
                 chrome_screen_ai::Direction direction,
                 int32_t block_id,
                 int32_t order_within_block,
                 float angle) {
  chrome_screen_ai::Rect* rect = line_box->mutable_bounding_box();
  rect->set_x(x);
  rect->set_y(y);
  rect->set_width(width);
  rect->set_height(height);
  rect->set_angle(angle);
  line_box->set_utf8_string(text);
  line_box->set_language(language);
  line_box->set_direction(direction);
  line_box->set_block_id(block_id);
  line_box->set_order_within_block(order_within_block);
}

void InitWordBox(chrome_screen_ai::WordBox* word_box,
                 int32_t x,
                 int32_t y,
                 int32_t width,
                 int32_t height,
                 const char* text,
                 const char* language,
                 chrome_screen_ai::Direction direction,
                 bool has_space_after,
                 int32_t background_rgb_value,
                 int32_t foreground_rgb_value,
                 float angle) {
  chrome_screen_ai::Rect* rect = word_box->mutable_bounding_box();
  rect->set_x(x);
  rect->set_y(y);
  rect->set_width(width);
  rect->set_height(height);
  rect->set_angle(angle);
  word_box->set_utf8_string(text);
  word_box->set_language(language);
  word_box->set_direction(direction);
  word_box->set_has_space_after(has_space_after);
  word_box->set_estimate_color_success(true);
  word_box->set_background_rgb_value(background_rgb_value);
  word_box->set_foreground_rgb_value(foreground_rgb_value);
}

void InitSymbolBox(chrome_screen_ai::SymbolBox* symbol_box,
                   int32_t x,
                   int32_t y,
                   int32_t width,
                   int32_t height,
                   const char* text) {
  chrome_screen_ai::Rect* rect = symbol_box->mutable_bounding_box();
  rect->set_x(x);
  rect->set_y(y);
  rect->set_width(width);
  rect->set_height(height);
  symbol_box->set_utf8_string(text);
}

}  // namespace

namespace screen_ai {

using ScreenAIVisualAnnotatorProtoConvertorTest = testing::Test;

TEST_F(ScreenAIVisualAnnotatorProtoConvertorTest,
       VisualAnnotationToAXTreeUpdate_OcrResults) {
  chrome_screen_ai::VisualAnnotation annotation;
  gfx::Rect snapshot_bounds(800, 900);

  screen_ai::ResetNodeIDForTesting();

  {
    chrome_screen_ai::LineBox* line_0 = annotation.add_lines();

    InitWordBox(line_0->add_words(),
                /*x=*/100,
                /*y=*/100,
                /*width=*/250,
                /*height=*/20,
                /*text=*/"Hello",
                /*language=*/"en",
                /*direction=*/chrome_screen_ai::DIRECTION_LEFT_TO_RIGHT,
                /*has_space_after=*/true,
                /*background_rgb_value=*/0xffffff00,
                /*foreground_rgb_value=*/0x00000000,  // Black on white.
                /*angle=*/0);

    InitWordBox(line_0->add_words(),
                /*x=*/350,
                /*y=*/100,
                /*width=*/250,
                /*height=*/20,
                /*text=*/"world",
                /*language=*/"en",
                /*direction=*/chrome_screen_ai::DIRECTION_LEFT_TO_RIGHT,
                /*has_space_after=*/false,
                /*background_rgb_value=*/0xffffff00,
                /*foreground_rgb_value=*/0xff000000,  // Blue on white.
                /*angle=*/0);

    InitLineBox(line_0,
                /*x=*/100,
                /*y=*/100,
                /*width=*/500,
                /*height=*/20,
                /*text=*/"Hello world",
                /*language=*/"en",
                /*direction=*/chrome_screen_ai::DIRECTION_LEFT_TO_RIGHT,
                /*block_id=*/1,
                /*order_within_block=*/1,
                /*angle=*/0);
  }

  {
    const ui::AXTreeUpdate update =
        VisualAnnotationToAXTreeUpdate(annotation, snapshot_bounds);

    const std::string expected_update(
        "AXTreeUpdate: root id -2\n"
        "id=-2 region class_name=ocred_page child_ids=-3,-5,-8 (0, 0)-(800, "
        "900) is_page_breaking_object=true\n"
        "  id=-3 banner child_ids=-4 (0, 0)-(1, 1)\n"
        "    id=-4 staticText name=Start of extracted text (0, 0)-(1, 1)\n"
        "  id=-5 paragraph child_ids=-6 (100, 100)-(500, 20)\n"
        "    id=-6 staticText name=Hello world child_ids=-7 "
        "(100, 100)-(500, 20) text_direction=ltr language=en\n"
        "      id=-7 inlineTextBox name=Hello world (100, 100)-(500, 20) "
        "background_color=&FFFFFF00 color=&0 text_direction=ltr "
        "word_starts=0,6 word_ends=5,10\n  id=-8 contentInfo child_ids=-9 "
        "(800, 900)-(1, 1)\n"
        "    id=-9 staticText name=End of extracted text (800, 900)-(1, 1)\n");
    EXPECT_EQ(expected_update, update.ToString());
  }
}

TEST_F(ScreenAIVisualAnnotatorProtoConvertorTest,
       VisualAnnotationToAXTreeUpdate_OcrResults_MultipleLanguages) {
  chrome_screen_ai::VisualAnnotation annotation;
  gfx::Rect snapshot_bounds(800, 900);

  screen_ai::ResetNodeIDForTesting();

  {
    chrome_screen_ai::LineBox* line_0 = annotation.add_lines();

    InitWordBox(line_0->add_words(),
                /*x=*/100,
                /*y=*/100,
                /*width=*/250,
                /*height=*/20,
                /*text=*/"Bonjour",
                /*language=*/"fr",
                /*direction=*/chrome_screen_ai::DIRECTION_LEFT_TO_RIGHT,
                /*has_space_after=*/true,
                /*background_rgb_value=*/0xffffff00,
                /*foreground_rgb_value=*/0x00000000,  // Black on white.
                /*angle=*/0);

    InitWordBox(line_0->add_words(),
                /*x=*/350,
                /*y=*/100,
                /*width=*/250,
                /*height=*/20,
                /*text=*/"world",
                /*language=*/"en",
                /*direction=*/chrome_screen_ai::DIRECTION_LEFT_TO_RIGHT,
                /*has_space_after=*/false,
                /*background_rgb_value=*/0xffffff00,
                /*foreground_rgb_value=*/0xff000000,  // Blue on white.
                /*angle=*/0);

    InitLineBox(line_0,
                /*x=*/100,
                /*y=*/100,
                /*width=*/500,
                /*height=*/20,
                /*text=*/"Bonjour world",
                /*language=*/"en",
                /*direction=*/chrome_screen_ai::DIRECTION_LEFT_TO_RIGHT,
                /*block_id=*/1,
                /*order_within_block=*/1,
                /*angle=*/0);
  }

  {
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
}

TEST_F(ScreenAIVisualAnnotatorProtoConvertorTest,
       ConvertProtoToVisualAnnotation_OcrResults) {
  chrome_screen_ai::VisualAnnotation annotation;

  {
    chrome_screen_ai::LineBox* line_0 = annotation.add_lines();

    InitWordBox(line_0->add_words(),
                /*x=*/100,
                /*y=*/100,
                /*width=*/250,
                /*height=*/20,
                /*text=*/"Hello",
                /*language=*/"en",
                /*direction=*/chrome_screen_ai::DIRECTION_LEFT_TO_RIGHT,
                /*has_space_after=*/true,
                /*background_rgb_value=*/0xffffff00,
                /*foreground_rgb_value=*/0x00000000,  // Black on white.
                /*angle=*/1);

    InitWordBox(line_0->add_words(),
                /*x=*/350,
                /*y=*/100,
                /*width=*/250,
                /*height=*/20,
                /*text=*/"world",
                /*language=*/"en",
                /*direction=*/chrome_screen_ai::DIRECTION_LEFT_TO_RIGHT,
                /*has_space_after=*/false,
                /*background_rgb_value=*/0xffffff00,
                /*foreground_rgb_value=*/0xff000000,  // Blue on white.
                /*angle=*/2);

    InitLineBox(line_0,
                /*x=*/100,
                /*y=*/100,
                /*width=*/500,
                /*height=*/20,
                /*text=*/"Hello world",
                /*language=*/"en",
                /*direction=*/chrome_screen_ai::DIRECTION_LEFT_TO_RIGHT,
                /*block_id=*/1,
                /*order_within_block=*/1,
                /*angle=*/1.5);

    chrome_screen_ai::Rect* line_baseline_box = line_0->mutable_baseline_box();
    line_baseline_box->set_x(110);
    line_baseline_box->set_y(110);
    line_baseline_box->set_width(510);
    line_baseline_box->set_height(30);
    line_baseline_box->set_angle(11.5);
  }

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
    EXPECT_EQ(line->baseline_box.x(), 110);
    EXPECT_EQ(line->baseline_box.y(), 110);
    EXPECT_EQ(line->baseline_box.width(), 510);
    EXPECT_EQ(line->baseline_box.height(), 30);
    EXPECT_EQ(line->baseline_box_angle, 11.5);
    EXPECT_EQ(line->text_line, "Hello world");
    EXPECT_EQ(line->block_id, 1);
    EXPECT_EQ(line->order_within_block, 1);
    EXPECT_EQ(line->words.size(), static_cast<unsigned long>(2));

    mojom::WordBoxPtr& word_0 = line->words[0];
    EXPECT_EQ(word_0->word, "Hello");
    EXPECT_EQ(word_0->dictionary_word, false);
    EXPECT_EQ(word_0->language, "en");
    EXPECT_EQ(word_0->has_space_after, true);
    EXPECT_EQ(word_0->bounding_box.x(), 100);
    EXPECT_EQ(word_0->bounding_box.y(), 100);
    EXPECT_EQ(word_0->bounding_box.width(), 250);
    EXPECT_EQ(word_0->bounding_box.height(), 20);
    EXPECT_EQ(word_0->bounding_box_angle, 1);
    EXPECT_EQ(word_0->direction, mojom::Direction::DIRECTION_LEFT_TO_RIGHT);

    mojom::WordBoxPtr& word_1 = line->words[1];
    EXPECT_EQ(word_1->word, "world");
    EXPECT_EQ(word_1->dictionary_word, false);
    EXPECT_EQ(word_1->language, "en");
    EXPECT_EQ(word_1->has_space_after, false);
    EXPECT_EQ(word_1->bounding_box.x(), 350);
    EXPECT_EQ(word_1->bounding_box.y(), 100);
    EXPECT_EQ(word_1->bounding_box.width(), 250);
    EXPECT_EQ(word_1->bounding_box.height(), 20);
    EXPECT_EQ(word_1->bounding_box_angle, 2);
    EXPECT_EQ(word_1->direction, mojom::Direction::DIRECTION_LEFT_TO_RIGHT);
  }
}

TEST_F(ScreenAIVisualAnnotatorProtoConvertorTest,
       VisualAnnotationToAXTreeUpdate_OcrResults_RightToLeftMultiByte) {
  chrome_screen_ai::VisualAnnotation annotation;
  gfx::Rect snapshot_bounds(800, 900);

  screen_ai::ResetNodeIDForTesting();

  {
    chrome_screen_ai::LineBox* line_0 = annotation.add_lines();
    chrome_screen_ai::WordBox* word_0 = line_0->add_words();
    chrome_screen_ai::WordBox* word_1 = line_0->add_words();

    InitSymbolBox(word_0->add_symbols(),
                  /*x=*/133,
                  /*y=*/100,
                  /*width=*/4,
                  /*height=*/20,
                  /*text=*/"ر");

    InitSymbolBox(word_0->add_symbols(),
                  /*x=*/129,
                  /*y=*/100,
                  /*width=*/4,
                  /*height=*/20,
                  /*text=*/"و");

    InitSymbolBox(word_0->add_symbols(),
                  /*x=*/125,
                  /*y=*/100,
                  /*width=*/4,
                  /*height=*/20,
                  /*text=*/"ز");

    InitWordBox(word_0,
                /*x=*/125,
                /*y=*/100,
                /*width=*/12,
                /*height=*/20,
                /*text=*/"روز",
                /*language=*/"fa",
                /*direction=*/chrome_screen_ai::DIRECTION_RIGHT_TO_LEFT,
                /*has_space_after=*/true,
                /*background_rgb_value=*/0xffffff00,
                /*foreground_rgb_value=*/0xff000000,  // Blue on white.
                /*angle=*/0);

    InitSymbolBox(word_1->add_symbols(),
                  /*x=*/114,
                  /*y=*/100,
                  /*width=*/6,
                  /*height=*/20,
                  /*text=*/"خ");

    InitSymbolBox(word_1->add_symbols(),
                  /*x=*/110,
                  /*y=*/4,
                  /*width=*/50,
                  /*height=*/20,
                  /*text=*/"و");

    InitSymbolBox(word_1->add_symbols(),
                  /*x=*/100,
                  /*y=*/100,
                  /*width=*/10,
                  /*height=*/20,
                  /*text=*/"ش");

    InitWordBox(word_1,
                /*x=*/100,
                /*y=*/100,
                /*width=*/20,
                /*height=*/20,
                /*text=*/"خوش",
                /*language=*/"fa",
                /*direction=*/chrome_screen_ai::DIRECTION_RIGHT_TO_LEFT,
                /*has_space_after=*/false,
                /*background_rgb_value=*/0xffffff00,
                /*foreground_rgb_value=*/0x00000000,  // Black on white.
                /*angle=*/0);

    InitLineBox(line_0,
                /*x=*/100,
                /*y=*/100,
                /*width=*/37,
                /*height=*/20,
                /*text=*/"روز بخیر",
                /*language=*/"fa",
                /*direction=*/chrome_screen_ai::DIRECTION_RIGHT_TO_LEFT,
                /*block_id=*/1,
                /*order_within_block=*/1,
                /*angle=*/0);
  }

  {
    const ui::AXTreeUpdate update =
        VisualAnnotationToAXTreeUpdate(annotation, snapshot_bounds);

    const std::string expected_update(
        "AXTreeUpdate: root id -2\n"
        "id=-2 region class_name=ocred_page child_ids=-3,-5,-8 (0, 0)-(800, "
        "900) is_page_breaking_object=true\n"
        "  id=-3 banner child_ids=-4 (0, 0)-(1, 1)\n"
        "    id=-4 staticText name=Start of extracted text (0, 0)-(1, 1)\n"
        "  id=-5 paragraph child_ids=-6 (100, 100)-(37, 20)\n"
        "    id=-6 staticText name=روز بخیر child_ids=-7 (100, 100)-(37, 20) "
        "text_direction=rtl language=fa\n"
        "      id=-7 inlineTextBox name=روز خوش (100, 100)-(37, 20) "
        "background_color=&FFFFFF00 color=&FF000000 text_direction=rtl "
        "character_offsets=4,8,12,17,23,27,37 word_starts=0,4 word_ends=3,6\n"
        "  id=-8 contentInfo child_ids=-9 (800, 900)-(1, 1)\n"
        "    id=-9 staticText name=End of extracted text (800, 900)-(1, 1)\n");
    EXPECT_EQ(expected_update, update.ToString());
  }
}

TEST_F(
    ScreenAIVisualAnnotatorProtoConvertorTest,
    VisualAnnotationToAXTreeUpdate_OcrResults_CharacterOffsets_LeftToRight_NotRotated) {
  chrome_screen_ai::VisualAnnotation annotation;
  gfx::Rect snapshot_bounds(800, 900);

  screen_ai::ResetNodeIDForTesting();

  {
    chrome_screen_ai::LineBox* line_0 = annotation.add_lines();
    chrome_screen_ai::WordBox* word_0 = line_0->add_words();
    chrome_screen_ai::WordBox* word_1 = line_0->add_words();

    InitSymbolBox(word_0->add_symbols(),
                  /*x=*/100,
                  /*y=*/100,
                  /*width=*/4,
                  /*height=*/19,
                  /*text=*/"D");

    InitSymbolBox(word_0->add_symbols(),
                  /*x=*/104,
                  /*y=*/100,
                  /*width=*/6,
                  /*height=*/19,
                  /*text=*/"a");

    InitSymbolBox(word_0->add_symbols(),
                  /*x=*/113,
                  /*y=*/100,
                  /*width=*/6,
                  /*height=*/19,
                  /*text=*/"y");

    InitWordBox(word_0,
                /*x=*/100,
                /*y=*/100,
                /*width=*/22,
                /*height=*/19,
                /*text=*/"Day",
                /*language=*/"en",
                /*direction=*/chrome_screen_ai::DIRECTION_LEFT_TO_RIGHT,
                /*has_space_after=*/true,
                /*background_rgb_value=*/0xffffff00,
                /*foreground_rgb_value=*/0xff000000,  // Blue on white.
                /*angle=*/0);

    InitSymbolBox(word_1->add_symbols(),
                  /*x=*/122,
                  /*y=*/100,
                  /*width=*/4,
                  /*height=*/19,
                  /*text=*/"O");

    InitSymbolBox(word_1->add_symbols(),
                  /*x=*/126,
                  /*y=*/100,
                  /*width=*/6,
                  /*height=*/19,
                  /*text=*/"n");

    InitSymbolBox(word_1->add_symbols(),
                  /*x=*/135,
                  /*y=*/100,
                  /*width=*/6,
                  /*height=*/19,
                  /*text=*/"e");

    InitWordBox(word_1,
                /*x=*/122,
                /*y=*/100,
                /*width=*/19,
                /*height=*/19,
                /*text=*/"One",
                /*language=*/"en",
                /*direction=*/chrome_screen_ai::DIRECTION_LEFT_TO_RIGHT,
                /*has_space_after=*/false,
                /*background_rgb_value=*/0xffffff00,
                /*foreground_rgb_value=*/0xff000000,  // Blue on white.
                /*angle=*/0);

    InitLineBox(line_0,
                /*x=*/100,
                /*y=*/100,
                /*width=*/41,
                /*height=*/19,
                /*text=*/"Day One",
                /*language=*/"en",
                /*direction=*/chrome_screen_ai::DIRECTION_LEFT_TO_RIGHT,
                /*block_id=*/1,
                /*order_within_block=*/1,
                /*angle=*/0);
  }

  {
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

TEST_F(
    ScreenAIVisualAnnotatorProtoConvertorTest,
    VisualAnnotationToAXTreeUpdate_OcrResults_CharacterOffsets_LeftToRight_Rotated) {
  chrome_screen_ai::VisualAnnotation annotation;
  gfx::Rect snapshot_bounds(800, 900);

  screen_ai::ResetNodeIDForTesting();

  {
    chrome_screen_ai::LineBox* line_0 = annotation.add_lines();
    chrome_screen_ai::WordBox* word_0 = line_0->add_words();
    chrome_screen_ai::WordBox* word_1 = line_0->add_words();

    InitSymbolBox(word_0->add_symbols(),
                  /*x=*/100,
                  /*y=*/100,
                  /*width=*/19,
                  /*height=*/4,
                  /*text=*/"D");

    InitSymbolBox(word_0->add_symbols(),
                  /*x=*/100,
                  /*y=*/104,
                  /*width=*/19,
                  /*height=*/6,
                  /*text=*/"a");

    InitSymbolBox(word_0->add_symbols(),
                  /*x=*/100,
                  /*y=*/113,
                  /*width=*/19,
                  /*height=*/6,
                  /*text=*/"y");

    InitWordBox(word_0,
                /*x=*/100,
                /*y=*/100,
                /*width=*/19,
                /*height=*/22,
                /*text=*/"Day",
                /*language=*/"en",
                /*direction=*/chrome_screen_ai::DIRECTION_LEFT_TO_RIGHT,
                /*has_space_after=*/true,
                /*background_rgb_value=*/0xffffff00,
                /*foreground_rgb_value=*/0xff000000,  // Blue on white.
                /*angle=*/90);

    InitSymbolBox(word_1->add_symbols(),
                  /*x=*/100,
                  /*y=*/122,
                  /*width=*/19,
                  /*height=*/4,
                  /*text=*/"O");

    InitSymbolBox(word_1->add_symbols(),
                  /*x=*/100,
                  /*y=*/126,
                  /*width=*/19,
                  /*height=*/6,
                  /*text=*/"n");

    InitSymbolBox(word_1->add_symbols(),
                  /*x=*/100,
                  /*y=*/135,
                  /*width=*/19,
                  /*height=*/6,
                  /*text=*/"e");

    InitWordBox(word_1,
                /*x=*/100,
                /*y=*/122,
                /*width=*/19,
                /*height=*/19,
                /*text=*/"One",
                /*language=*/"en",
                /*direction=*/chrome_screen_ai::DIRECTION_LEFT_TO_RIGHT,
                /*has_space_after=*/false,
                /*background_rgb_value=*/0xffffff00,
                /*foreground_rgb_value=*/0xff000000,  // Blue on white.
                /*angle=*/90);

    InitLineBox(line_0,
                /*x=*/100,
                /*y=*/100,
                /*width=*/19,
                /*height=*/41,
                /*text=*/"Day One",
                /*language=*/"en",
                /*direction=*/chrome_screen_ai::DIRECTION_LEFT_TO_RIGHT,
                /*block_id=*/1,
                /*order_within_block=*/1,
                /*angle=*/90);
  }

  {
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

}  // namespace screen_ai
