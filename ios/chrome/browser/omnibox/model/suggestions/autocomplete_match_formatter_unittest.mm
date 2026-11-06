// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/suggestions/autocomplete_match_formatter.h"

#import <UIKit/UIKit.h>

#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/autocomplete_match_test_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/omnibox_proto/formatted_string.pb.h"
#import "third_party/omnibox_proto/suggest_template_info.pb.h"

namespace {

using AutocompleteMatchFormatterTest = PlatformTest;

// Adds a fragment that starts at `start_index` to `formatted_string`.
void AddFragment(omnibox::FormattedString* formatted_string,
                 uint32_t start_index,
                 omnibox::FormattedString::ColorType color,
                 bool is_bolded = false) {
  omnibox::FormattedString::FormattedStringFragment* fragment =
      formatted_string->add_fragments();
  fragment->set_start_index(start_index);
  fragment->set_color(color);
  fragment->set_is_bolded(is_bolded);
}

// Returns the parts of `text` in the color named `color_name`, separated by
// commas.
NSString* PartsWithColor(NSAttributedString* text, NSString* color_name) {
  UIColor* color = [UIColor colorNamed:color_name];
  NSMutableArray<NSString*>* parts = [NSMutableArray array];
  [text enumerateAttribute:NSForegroundColorAttributeName
                   inRange:NSMakeRange(0, text.length)
                   options:0
                usingBlock:^(id value, NSRange range, BOOL* stop) {
                  if ([value isEqual:color]) {
                    [parts addObject:[text.string substringWithRange:range]];
                  }
                }];
  return [parts componentsJoinedByString:@","];
}

// Returns the parts of `text` in a bold font, separated by commas.
NSString* BoldParts(NSAttributedString* text) {
  NSMutableArray<NSString*>* parts = [NSMutableArray array];
  [text enumerateAttribute:NSFontAttributeName
                   inRange:NSMakeRange(0, text.length)
                   options:0
                usingBlock:^(id value, NSRange range, BOOL* stop) {
                  UIFont* font = value;
                  if (font.fontDescriptor.symbolicTraits &
                      UIFontDescriptorTraitBold) {
                    [parts addObject:[text.string substringWithRange:range]];
                  }
                }];
  return [parts componentsJoinedByString:@","];
}

// Tests that the text of a search suggestion takes the server's colors and
// bolding.
TEST_F(AutocompleteMatchFormatterTest,
       TextTakesFormattedStringColorsAndBolding) {
  AutocompleteMatch match = CreateSearchMatch(u"pizza near me");
  omnibox::FormattedString* primary_text =
      match.suggest_template.emplace().mutable_primary_text();
  primary_text->set_text("pizza near me");
  AddFragment(primary_text, 0,
              omnibox::FormattedString::COLOR_SUGGEST_HINT_TEXT,
              /*is_bolded=*/true);
  AddFragment(primary_text, 5, omnibox::FormattedString::COLOR_UNKNOWN);
  AddFragment(primary_text, 11,
              omnibox::FormattedString::COLOR_SUGGEST_ACTION_HIGH);

  NSAttributedString* text =
      [AutocompleteMatchFormatter formatterWithMatch:match].text;

  EXPECT_NSEQ(@"pizza", PartsWithColor(text, kTextSecondaryColor));
  EXPECT_NSEQ(@" near ", PartsWithColor(text, kTextPrimaryColor));
  EXPECT_NSEQ(@"me", PartsWithColor(text, kBlueColor));
  EXPECT_NSEQ(@"pizza", BoldParts(text));
}

// Tests that the detail text of a search suggestion takes the server's colors
// and bolding.
TEST_F(AutocompleteMatchFormatterTest,
       DetailTextTakesFormattedStringColorsAndBolding) {
  AutocompleteMatch match = CreateSearchMatch(u"pizza");
  omnibox::FormattedString* secondary_text =
      match.suggest_template.emplace().mutable_secondary_text();
  secondary_text->set_text("Open now - Closes 9 PM");
  AddFragment(secondary_text, 0,
              omnibox::FormattedString::COLOR_SUGGEST_QUERY_TEXT,
              /*is_bolded=*/true);
  AddFragment(secondary_text, 8, omnibox::FormattedString::COLOR_UNKNOWN);

  NSAttributedString* detail_text =
      [AutocompleteMatchFormatter formatterWithMatch:match].detailText;

  EXPECT_NSEQ(@"Open now", PartsWithColor(detail_text, kTextPrimaryColor));
  EXPECT_NSEQ(@" - Closes 9 PM",
              PartsWithColor(detail_text, kTextSecondaryColor));
  EXPECT_NSEQ(@"Open now", BoldParts(detail_text));
}

// Tests that text that differs from the server's text keeps its colors.
TEST_F(AutocompleteMatchFormatterTest, IgnoresFormattedStringWithOtherText) {
  AutocompleteMatch match = CreateSearchMatch(u"pizza near me");
  omnibox::SuggestTemplateInfo& suggest_template =
      match.suggest_template.emplace();
  omnibox::FormattedString* primary_text =
      suggest_template.mutable_primary_text();
  primary_text->set_text("Pizza near me");
  AddFragment(primary_text, 0,
              omnibox::FormattedString::COLOR_SUGGEST_HINT_TEXT);
  omnibox::FormattedString* secondary_text =
      suggest_template.mutable_secondary_text();
  secondary_text->set_text("Open now");
  AddFragment(secondary_text, 0,
              omnibox::FormattedString::COLOR_SUGGEST_QUERY_TEXT);

  AutocompleteMatchFormatter* formatter =
      [AutocompleteMatchFormatter formatterWithMatch:match];

  EXPECT_NSEQ(@"pizza near me",
              PartsWithColor(formatter.text, kTextPrimaryColor));
  // The detail text is the server's text, so it takes the server's colors.
  EXPECT_NSEQ(@"Open now",
              PartsWithColor(formatter.detailText, kTextPrimaryColor));
}

// Tests that types without a dedicated suggestion text color, including future
// ones, get the primary text color.
TEST_F(AutocompleteMatchFormatterTest, UsesPrimaryTextColorForOtherColorTypes) {
  AutocompleteMatch match = CreateSearchMatch(u"pizza");
  omnibox::FormattedString* secondary_text =
      match.suggest_template.emplace().mutable_secondary_text();
  secondary_text->set_text("Open now - Closes 9 PM");
  AddFragment(secondary_text, 0, omnibox::FormattedString::COLOR_PRIMARY);
  AddFragment(secondary_text, 8, omnibox::FormattedString::COLOR_UNKNOWN);

  NSAttributedString* detail_text =
      [AutocompleteMatchFormatter formatterWithMatch:match].detailText;

  EXPECT_NSEQ(@"Open now", PartsWithColor(detail_text, kTextPrimaryColor));
  EXPECT_NSEQ(@" - Closes 9 PM",
              PartsWithColor(detail_text, kTextSecondaryColor));
}

// Tests that a fragment without a start index doesn't drop the fragment before
// it.
TEST_F(AutocompleteMatchFormatterTest, SkipsFragmentWithoutStartIndex) {
  AutocompleteMatch match = CreateSearchMatch(u"pizza");
  omnibox::FormattedString* secondary_text =
      match.suggest_template.emplace().mutable_secondary_text();
  secondary_text->set_text("Pizza - Open now");
  AddFragment(secondary_text, 0, omnibox::FormattedString::COLOR_UNKNOWN);
  AddFragment(secondary_text, 8,
              omnibox::FormattedString::COLOR_SUGGEST_QUERY_TEXT,
              /*is_bolded=*/true);
  secondary_text->add_fragments()->set_text("ignored");

  NSAttributedString* detail_text =
      [AutocompleteMatchFormatter formatterWithMatch:match].detailText;

  EXPECT_NSEQ(@"Open now", PartsWithColor(detail_text, kTextPrimaryColor));
  EXPECT_NSEQ(@"Open now", BoldParts(detail_text));
}

// Tests that a fragment that starts past the end of the text doesn't drop the
// fragment before it.
TEST_F(AutocompleteMatchFormatterTest, ClampsFragmentEndToTextLength) {
  AutocompleteMatch match = CreateSearchMatch(u"pizza");
  omnibox::FormattedString* secondary_text =
      match.suggest_template.emplace().mutable_secondary_text();
  secondary_text->set_text("Pizza - Open now");
  AddFragment(secondary_text, 0, omnibox::FormattedString::COLOR_UNKNOWN);
  AddFragment(secondary_text, 8,
              omnibox::FormattedString::COLOR_SUGGEST_QUERY_TEXT,
              /*is_bolded=*/true);
  AddFragment(secondary_text, 50, omnibox::FormattedString::COLOR_UNKNOWN);

  NSAttributedString* detail_text =
      [AutocompleteMatchFormatter formatterWithMatch:match].detailText;

  EXPECT_NSEQ(@"Open now", PartsWithColor(detail_text, kTextPrimaryColor));
  EXPECT_NSEQ(@"Open now", BoldParts(detail_text));
}

}  // namespace
