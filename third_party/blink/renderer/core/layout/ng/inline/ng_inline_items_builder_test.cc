// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_items_builder.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/ng/inline/layout_ng_text.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node_data.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_test.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

// The spec turned into a discussion that may change. Put this logic on hold
// until CSSWG resolves the issue.
// https://github.com/w3c/csswg-drafts/issues/337
#define SEGMENT_BREAK_TRANSFORMATION_FOR_EAST_ASIAN_WIDTH 0

#define EXPECT_ITEM_OFFSET(item, type, start, end) \
  EXPECT_EQ(type, (item).Type());                  \
  EXPECT_EQ(start, (item).StartOffset());          \
  EXPECT_EQ(end, (item).EndOffset());

class NGInlineItemsBuilderTest : public NGLayoutTest {
 protected:
  void SetUp() override {
    NGLayoutTest::SetUp();
    style_ = ComputedStyle::Create();
    style_->GetFont().Update(nullptr);
  }

  void TearDown() override {
    for (LayoutObject* anonymous_object : anonymous_objects_)
      anonymous_object->Destroy();
    NGLayoutTest::TearDown();
  }

  void SetWhiteSpace(EWhiteSpace whitespace) {
    style_->SetWhiteSpace(whitespace);
  }

  scoped_refptr<ComputedStyle> GetStyle(EWhiteSpace whitespace) {
    if (whitespace == EWhiteSpace::kNormal)
      return style_;
    scoped_refptr<ComputedStyle> style(ComputedStyle::Create());
    style->SetWhiteSpace(whitespace);
    return style;
  }

  void AppendText(const String& text, NGInlineItemsBuilder* builder) {
    LayoutText* layout_text = LayoutText::CreateEmptyAnonymous(
        GetDocument(), style_.get(), LegacyLayout::kAuto);
    anonymous_objects_.push_back(layout_text);
    builder->AppendText(text, layout_text);
  }

  void AppendAtomicInline(NGInlineItemsBuilder* builder) {
    LayoutBlockFlow* layout_block_flow = LayoutBlockFlow::CreateAnonymous(
        &GetDocument(), style_, LegacyLayout::kAuto);
    anonymous_objects_.push_back(layout_block_flow);
    builder->AppendAtomicInline(layout_block_flow);
  }

  struct Input {
    const String text;
    EWhiteSpace whitespace = EWhiteSpace::kNormal;
    LayoutText* layout_text = nullptr;
  };

  const String& TestAppend(Vector<Input> inputs) {
    items_.clear();
    Vector<LayoutText*> anonymous_objects;
    NGInlineItemsBuilder builder(&items_);
    for (Input& input : inputs) {
      if (!input.layout_text) {
        input.layout_text = LayoutText::CreateEmptyAnonymous(
            GetDocument(), GetStyle(input.whitespace), LegacyLayout::kAuto);
        anonymous_objects.push_back(input.layout_text);
      }
      builder.AppendText(input.text, input.layout_text);
    }
    builder.ExitBlock();
    text_ = builder.ToString();
    ValidateItems();
    CheckReuseItemsProducesSameResult(inputs, builder.HasBidiControls());
    for (LayoutObject* anonymous_object : anonymous_objects)
      anonymous_object->Destroy();
    return text_;
  }

  const String& TestAppend(const String& input) {
    return TestAppend({Input{input}});
  }
  const String& TestAppend(const Input& input1, const Input& input2) {
    return TestAppend({input1, input2});
  }
  const String& TestAppend(const String& input1, const String& input2) {
    return TestAppend(Input{input1}, Input{input2});
  }
  const String& TestAppend(const String& input1,
                           const String& input2,
                           const String& input3) {
    return TestAppend({{input1}, {input2}, {input3}});
  }

  void ValidateItems() {
    unsigned current_offset = 0;
    for (unsigned i = 0; i < items_.size(); i++) {
      const NGInlineItem& item = items_[i];
      EXPECT_EQ(current_offset, item.StartOffset());
      EXPECT_LE(item.StartOffset(), item.EndOffset());
      current_offset = item.EndOffset();
    }
    EXPECT_EQ(current_offset, text_.length());
  }

  void CheckReuseItemsProducesSameResult(Vector<Input> inputs,
                                         bool has_bidi_controls) {
    NGInlineNodeData fake_data;
    fake_data.text_content = text_;
    fake_data.is_bidi_enabled_ = has_bidi_controls;

    Vector<NGInlineItem> reuse_items;
    NGInlineItemsBuilder reuse_builder(&reuse_items);
    for (Input& input : inputs) {
      // Collect items for this LayoutObject.
      DCHECK(input.layout_text);
      for (NGInlineItem* item = items_.begin(); item != items_.end();) {
        if (item->GetLayoutObject() == input.layout_text) {
          NGInlineItem* begin = item;
          for (++item; item != items_.end(); ++item) {
            if (item->GetLayoutObject() != input.layout_text)
              break;
          }
          input.layout_text->SetInlineItems(begin, item);
        } else {
          ++item;
        }
      }

      // Try to re-use previous items, or Append if it was not re-usable.
      bool reused =
          input.layout_text->HasValidInlineItems() &&
          reuse_builder.AppendTextReusing(fake_data, input.layout_text);
      if (!reused) {
        reuse_builder.AppendText(input.text, input.layout_text);
      }
    }

    reuse_builder.ExitBlock();
    String reuse_text = reuse_builder.ToString();
    EXPECT_EQ(text_, reuse_text);
  }

  Vector<NGInlineItem> items_;
  String text_;
  scoped_refptr<ComputedStyle> style_;
  Vector<LayoutObject*> anonymous_objects_;
};

#define TestWhitespaceValue(expected_text, input, whitespace) \
  SetWhiteSpace(whitespace);                                  \
  EXPECT_EQ(expected_text, TestAppend(input)) << "white-space: " #whitespace;

TEST_F(NGInlineItemsBuilderTest, CollapseSpaces) {
  String input("text text  text   text");
  String collapsed("text text text text");
  TestWhitespaceValue(collapsed, input, EWhiteSpace::kNormal);
  TestWhitespaceValue(collapsed, input, EWhiteSpace::kNowrap);
  TestWhitespaceValue(collapsed, input, EWhiteSpace::kWebkitNowrap);
  TestWhitespaceValue(collapsed, input, EWhiteSpace::kPreLine);
  TestWhitespaceValue(input, input, EWhiteSpace::kPre);
  TestWhitespaceValue(input, input, EWhiteSpace::kPreWrap);
}

TEST_F(NGInlineItemsBuilderTest, CollapseTabs) {
  String input("text text  text   text");
  String collapsed("text text text text");
  TestWhitespaceValue(collapsed, input, EWhiteSpace::kNormal);
  TestWhitespaceValue(collapsed, input, EWhiteSpace::kNowrap);
  TestWhitespaceValue(collapsed, input, EWhiteSpace::kWebkitNowrap);
  TestWhitespaceValue(collapsed, input, EWhiteSpace::kPreLine);
  TestWhitespaceValue(input, input, EWhiteSpace::kPre);
  TestWhitespaceValue(input, input, EWhiteSpace::kPreWrap);
}

TEST_F(NGInlineItemsBuilderTest, CollapseNewLines) {
  String input("text\ntext \ntext\n\ntext");
  String collapsed("text text text text");
  TestWhitespaceValue(collapsed, input, EWhiteSpace::kNormal);
  TestWhitespaceValue(collapsed, input, EWhiteSpace::kNowrap);
  TestWhitespaceValue("text\ntext\ntext\n\ntext", input, EWhiteSpace::kPreLine);
  TestWhitespaceValue(input, input, EWhiteSpace::kPre);
  TestWhitespaceValue(input, input, EWhiteSpace::kPreWrap);
}

TEST_F(NGInlineItemsBuilderTest, CollapseNewlinesAsSpaces) {
  EXPECT_EQ("text text", TestAppend("text\ntext"));
  EXPECT_EQ("text text", TestAppend("text\n\ntext"));
  EXPECT_EQ("text text", TestAppend("text \n\n text"));
  EXPECT_EQ("text text", TestAppend("text \n \n text"));
}

TEST_F(NGInlineItemsBuilderTest, CollapseAcrossElements) {
  EXPECT_EQ("text text", TestAppend("text ", " text"))
      << "Spaces are collapsed even when across elements.";
}

TEST_F(NGInlineItemsBuilderTest, CollapseLeadingSpaces) {
  EXPECT_EQ("text", TestAppend("  text"));
  EXPECT_EQ("text", TestAppend(" ", "text"));
  EXPECT_EQ("text", TestAppend(" ", " text"));
}

TEST_F(NGInlineItemsBuilderTest, CollapseTrailingSpaces) {
  EXPECT_EQ("text", TestAppend("text  "));
  EXPECT_EQ("text", TestAppend("text", " "));
  EXPECT_EQ("text", TestAppend("text ", " "));
}

TEST_F(NGInlineItemsBuilderTest, CollapseAllSpaces) {
  EXPECT_EQ("", TestAppend("  "));
  EXPECT_EQ("", TestAppend("  ", "  "));
  EXPECT_EQ("", TestAppend("  ", "\n"));
  EXPECT_EQ("", TestAppend("\n", "  "));
}

TEST_F(NGInlineItemsBuilderTest, CollapseLeadingNewlines) {
  EXPECT_EQ("text", TestAppend("\ntext"));
  EXPECT_EQ("text", TestAppend("\n\ntext"));
  EXPECT_EQ("text", TestAppend("\n", "text"));
  EXPECT_EQ("text", TestAppend("\n\n", "text"));
  EXPECT_EQ("text", TestAppend(" \n", "text"));
  EXPECT_EQ("text", TestAppend("\n", " text"));
  EXPECT_EQ("text", TestAppend("\n\n", " text"));
  EXPECT_EQ("text", TestAppend(" \n", " text"));
  EXPECT_EQ("text", TestAppend("\n", "\ntext"));
  EXPECT_EQ("text", TestAppend("\n\n", "\ntext"));
  EXPECT_EQ("text", TestAppend(" \n", "\ntext"));
}

TEST_F(NGInlineItemsBuilderTest, CollapseTrailingNewlines) {
  EXPECT_EQ("text", TestAppend("text\n"));
  EXPECT_EQ("text", TestAppend("text", "\n"));
  EXPECT_EQ("text", TestAppend("text\n", "\n"));
  EXPECT_EQ("text", TestAppend("text\n", " "));
  EXPECT_EQ("text", TestAppend("text ", "\n"));
}

TEST_F(NGInlineItemsBuilderTest, CollapseNewlineAcrossElements) {
  EXPECT_EQ("text text", TestAppend("text ", "\ntext"));
  EXPECT_EQ("text text", TestAppend("text ", "\n text"));
  EXPECT_EQ("text text", TestAppend("text", " ", "\ntext"));
}

TEST_F(NGInlineItemsBuilderTest, CollapseBeforeAndAfterNewline) {
  SetWhiteSpace(EWhiteSpace::kPreLine);
  EXPECT_EQ("text\ntext", TestAppend("text  \n  text"))
      << "Spaces before and after newline are removed.";
}

TEST_F(NGInlineItemsBuilderTest,
       CollapsibleSpaceAfterNonCollapsibleSpaceAcrossElements) {
  EXPECT_EQ("text  text",
            TestAppend({"text ", EWhiteSpace::kPreWrap}, {" text"}))
      << "The whitespace in constructions like '<span style=\"white-space: "
         "pre-wrap\">text <span><span> text</span>' does not collapse.";
}

TEST_F(NGInlineItemsBuilderTest, CollapseZeroWidthSpaces) {
  EXPECT_EQ(String(u"text\u200Btext"), TestAppend(u"text\u200B\ntext"))
      << "Newline is removed if the character before is ZWS.";
  EXPECT_EQ(String(u"text\u200Btext"), TestAppend(u"text\n\u200Btext"))
      << "Newline is removed if the character after is ZWS.";
  EXPECT_EQ(String(u"text\u200B\u200Btext"),
            TestAppend(u"text\u200B\n\u200Btext"))
      << "Newline is removed if the character before/after is ZWS.";

  EXPECT_EQ(String(u"text\u200Btext"), TestAppend(u"text\n", u"\u200Btext"))
      << "Newline is removed if the character after across elements is ZWS.";
  EXPECT_EQ(String(u"text\u200Btext"), TestAppend(u"text\u200B", u"\ntext"))
      << "Newline is removed if the character before is ZWS even across "
         "elements.";

  EXPECT_EQ(String(u"text\u200Btext"), TestAppend(u"text \n", u"\u200Btext"))
      << "Collapsible space before newline does not affect the result.";
  EXPECT_EQ(String(u"text\u200B text"), TestAppend(u"text\u200B\n", u" text"))
      << "Collapsible space after newline is removed even when the "
         "newline was removed.";
  EXPECT_EQ(String(u"text\u200Btext"), TestAppend(u"text\u200B ", u"\ntext"))
      << "A white space sequence containing a segment break before or after "
         "a zero width space is collapsed to a zero width space.";
}

TEST_F(NGInlineItemsBuilderTest, CollapseZeroWidthSpaceAndNewLineAtEnd) {
  EXPECT_EQ(String(u"\u200B"), TestAppend(u"\u200B\n"));
  EXPECT_EQ(NGInlineItem::kNotCollapsible, items_[0].EndCollapseType());
}

#if SEGMENT_BREAK_TRANSFORMATION_FOR_EAST_ASIAN_WIDTH
TEST_F(NGInlineItemsBuilderTest, CollapseEastAsianWidth) {
  EXPECT_EQ(String(u"\u4E00\u4E00"), TestAppend(u"\u4E00\n\u4E00"))
      << "Newline is removed when both sides are Wide.";

  EXPECT_EQ(String(u"\u4E00 A"), TestAppend(u"\u4E00\nA"))
      << "Newline is not removed when after is Narrow.";
  EXPECT_EQ(String(u"A \u4E00"), TestAppend(u"A\n\u4E00"))
      << "Newline is not removed when before is Narrow.";

  EXPECT_EQ(String(u"\u4E00\u4E00"), TestAppend(u"\u4E00\n", u"\u4E00"))
      << "Newline at the end of elements is removed when both sides are Wide.";
  EXPECT_EQ(String(u"\u4E00\u4E00"), TestAppend(u"\u4E00", u"\n\u4E00"))
      << "Newline at the beginning of elements is removed "
         "when both sides are Wide.";
}
#endif

TEST_F(NGInlineItemsBuilderTest, OpaqueToSpaceCollapsing) {
  NGInlineItemsBuilder builder(&items_);
  AppendText("Hello ", &builder);
  builder.AppendOpaque(NGInlineItem::kBidiControl,
                       kFirstStrongIsolateCharacter);
  AppendText(" ", &builder);
  builder.AppendOpaque(NGInlineItem::kBidiControl,
                       kFirstStrongIsolateCharacter);
  AppendText(" World", &builder);
  EXPECT_EQ(String(u"Hello \u2068\u2068World"), builder.ToString());
}

TEST_F(NGInlineItemsBuilderTest, CollapseAroundReplacedElement) {
  NGInlineItemsBuilder builder(&items_);
  AppendText("Hello ", &builder);
  AppendAtomicInline(&builder);
  AppendText(" World", &builder);
  EXPECT_EQ(String(u"Hello \uFFFC World"), builder.ToString());
}

TEST_F(NGInlineItemsBuilderTest, CollapseNewlineAfterObject) {
  NGInlineItemsBuilder builder(&items_);
  AppendAtomicInline(&builder);
  AppendText("\n", &builder);
  AppendAtomicInline(&builder);
  EXPECT_EQ(String(u"\uFFFC \uFFFC"), builder.ToString());
  EXPECT_EQ(3u, items_.size());
  EXPECT_ITEM_OFFSET(items_[0], NGInlineItem::kAtomicInline, 0u, 1u);
  EXPECT_ITEM_OFFSET(items_[1], NGInlineItem::kText, 1u, 2u);
  EXPECT_ITEM_OFFSET(items_[2], NGInlineItem::kAtomicInline, 2u, 3u);
}

TEST_F(NGInlineItemsBuilderTest, AppendEmptyString) {
  EXPECT_EQ("", TestAppend(""));
  EXPECT_EQ(1u, items_.size());
  EXPECT_ITEM_OFFSET(items_[0], NGInlineItem::kText, 0u, 0u);
}

TEST_F(NGInlineItemsBuilderTest, NewLines) {
  SetWhiteSpace(EWhiteSpace::kPre);
  EXPECT_EQ("apple\norange\ngrape\n", TestAppend("apple\norange\ngrape\n"));
  EXPECT_EQ(6u, items_.size());
  EXPECT_EQ(NGInlineItem::kText, items_[0].Type());
  EXPECT_EQ(NGInlineItem::kControl, items_[1].Type());
  EXPECT_EQ(NGInlineItem::kText, items_[2].Type());
  EXPECT_EQ(NGInlineItem::kControl, items_[3].Type());
  EXPECT_EQ(NGInlineItem::kText, items_[4].Type());
  EXPECT_EQ(NGInlineItem::kControl, items_[5].Type());
}

TEST_F(NGInlineItemsBuilderTest, IgnorablePre) {
  SetWhiteSpace(EWhiteSpace::kPre);
  EXPECT_EQ(
      "apple"
      "\x0c"
      "orange"
      "\n"
      "grape",
      TestAppend("apple"
                 "\x0c"
                 "orange"
                 "\n"
                 "grape"));
  EXPECT_EQ(5u, items_.size());
  EXPECT_ITEM_OFFSET(items_[0], NGInlineItem::kText, 0u, 5u);
  EXPECT_ITEM_OFFSET(items_[1], NGInlineItem::kControl, 5u, 6u);
  EXPECT_ITEM_OFFSET(items_[2], NGInlineItem::kText, 6u, 12u);
  EXPECT_ITEM_OFFSET(items_[3], NGInlineItem::kControl, 12u, 13u);
  EXPECT_ITEM_OFFSET(items_[4], NGInlineItem::kText, 13u, 18u);
}

TEST_F(NGInlineItemsBuilderTest, Empty) {
  Vector<NGInlineItem> items;
  NGInlineItemsBuilder builder(&items);
  scoped_refptr<ComputedStyle> block_style(ComputedStyle::Create());
  builder.EnterBlock(block_style.get());
  builder.ExitBlock();

  EXPECT_EQ("", builder.ToString());
}

class CollapsibleSpaceTest : public NGInlineItemsBuilderTest,
                             public testing::WithParamInterface<UChar> {};

INSTANTIATE_TEST_SUITE_P(NGInlineItemsBuilderTest,
                         CollapsibleSpaceTest,
                         testing::Values(kSpaceCharacter,
                                         kTabulationCharacter,
                                         kNewlineCharacter));

TEST_P(CollapsibleSpaceTest, CollapsedSpaceAfterNoWrap) {
  UChar space = GetParam();
  EXPECT_EQ(
      String("nowrap "
             u"\u200B"
             "wrap"),
      TestAppend({String("nowrap") + space, EWhiteSpace::kNowrap}, {" wrap"}));
}

TEST_F(NGInlineItemsBuilderTest, GenerateBreakOpportunityAfterLeadingSpaces) {
  EXPECT_EQ(String(" "
                   u"\u200B"
                   "a"),
            TestAppend({{" a", EWhiteSpace::kPreWrap}}));
  EXPECT_EQ(String("  "
                   u"\u200B"
                   "a"),
            TestAppend({{"  a", EWhiteSpace::kPreWrap}}));
  EXPECT_EQ(String("a\n"
                   u" \u200B"),
            TestAppend({{"a\n ", EWhiteSpace::kPreWrap}}));
}

TEST_F(NGInlineItemsBuilderTest, BidiBlockOverride) {
  Vector<NGInlineItem> items;
  NGInlineItemsBuilder builder(&items);
  scoped_refptr<ComputedStyle> block_style(ComputedStyle::Create());
  block_style->SetUnicodeBidi(UnicodeBidi::kBidiOverride);
  block_style->SetDirection(TextDirection::kRtl);
  builder.EnterBlock(block_style.get());
  AppendText("Hello", &builder);
  builder.ExitBlock();

  // Expected control characters as defined in:
  // https://drafts.csswg.org/css-writing-modes-3/#bidi-control-codes-injection-table
  EXPECT_EQ(String(u"\u202E"
                   u"Hello"
                   u"\u202C"),
            builder.ToString());
}

static std::unique_ptr<LayoutInline> CreateLayoutInline(
    void (*initialize_style)(ComputedStyle*)) {
  scoped_refptr<ComputedStyle> style(ComputedStyle::Create());
  initialize_style(style.get());
  std::unique_ptr<LayoutInline> node = std::make_unique<LayoutInline>(nullptr);
  node->SetModifiedStyleOutsideStyleRecalc(
      std::move(style), LayoutObject::ApplyStyleChanges::kNo);
  node->SetIsInLayoutNGInlineFormattingContext(true);
  return node;
}

TEST_F(NGInlineItemsBuilderTest, BidiIsolate) {
  Vector<NGInlineItem> items;
  NGInlineItemsBuilder builder(&items);
  AppendText("Hello ", &builder);
  std::unique_ptr<LayoutInline> isolate_rtl(
      CreateLayoutInline([](ComputedStyle* style) {
        style->SetUnicodeBidi(UnicodeBidi::kIsolate);
        style->SetDirection(TextDirection::kRtl);
      }));
  builder.EnterInline(isolate_rtl.get());
  AppendText(u"\u05E2\u05D1\u05E8\u05D9\u05EA", &builder);
  builder.ExitInline(isolate_rtl.get());
  AppendText(" World", &builder);

  // Expected control characters as defined in:
  // https://drafts.csswg.org/css-writing-modes-3/#bidi-control-codes-injection-table
  EXPECT_EQ(String(u"Hello "
                   u"\u2067"
                   u"\u05E2\u05D1\u05E8\u05D9\u05EA"
                   u"\u2069"
                   u" World"),
            builder.ToString());
}

TEST_F(NGInlineItemsBuilderTest, BidiIsolateOverride) {
  Vector<NGInlineItem> items;
  NGInlineItemsBuilder builder(&items);
  AppendText("Hello ", &builder);
  std::unique_ptr<LayoutInline> isolate_override_rtl(
      CreateLayoutInline([](ComputedStyle* style) {
        style->SetUnicodeBidi(UnicodeBidi::kIsolateOverride);
        style->SetDirection(TextDirection::kRtl);
      }));
  builder.EnterInline(isolate_override_rtl.get());
  AppendText(u"\u05E2\u05D1\u05E8\u05D9\u05EA", &builder);
  builder.ExitInline(isolate_override_rtl.get());
  AppendText(" World", &builder);

  // Expected control characters as defined in:
  // https://drafts.csswg.org/css-writing-modes-3/#bidi-control-codes-injection-table
  EXPECT_EQ(String(u"Hello "
                   u"\u2068\u202E"
                   u"\u05E2\u05D1\u05E8\u05D9\u05EA"
                   u"\u202C\u2069"
                   u" World"),
            builder.ToString());
}

}  // namespace blink
