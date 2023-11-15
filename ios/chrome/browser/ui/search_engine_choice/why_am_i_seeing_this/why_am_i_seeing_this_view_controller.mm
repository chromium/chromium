// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/search_engine_choice/why_am_i_seeing_this/why_am_i_seeing_this_view_controller.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_attributed_string_header_footer_item.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Accessibility Identifier.
NSString* const kWhyAmISeeingThisTitleAccessibilityIdentifier =
    @"WhyAmISeeingThisTitleAccessibilityIdentifier";
// Empty line between two paragraphs.
NSString* const kEmptyLine = @"\n \n";
//
NSString* const kBeginBoldTag = @"<b>";
NSString* const kEndBoldTag = @"</b>";

// The logic here is taken from PutBoldPartInString in
// "ios/chrome/common/string_util.h" except that we use desktop-style beginning
// and end bold tags.
NSAttributedString* PutBoldPartInText(NSString* string) {
  UIFontDescriptor* default_descriptor = [UIFontDescriptor
      preferredFontDescriptorWithTextStyle:UIFontTextStyleFootnote];
  UIFontDescriptor* bold_descriptor = [[UIFontDescriptor
      preferredFontDescriptorWithTextStyle:UIFontTextStyleFootnote]
      fontDescriptorWithSymbolicTraits:UIFontDescriptorTraitBold];
  StringWithTag parsed_string =
      ParseStringWithTag(string, kBeginBoldTag, kEndBoldTag);

  NSMutableAttributedString* attributed_string =
      [[NSMutableAttributedString alloc] initWithString:parsed_string.string];
  [attributed_string addAttribute:NSFontAttributeName
                            value:[UIFont fontWithDescriptor:default_descriptor
                                                        size:0.0]
                            range:NSMakeRange(0, parsed_string.string.length)];

  [attributed_string addAttribute:NSFontAttributeName
                            value:[UIFont fontWithDescriptor:bold_descriptor
                                                        size:0.0]
                            range:parsed_string.range];
  [attributed_string addAttribute:NSForegroundColorAttributeName
                            value:[UIColor colorNamed:kSolidBlackColor]
                            range:NSMakeRange(0, parsed_string.string.length)];

  return attributed_string;
}

}  // namespace

@implementation WhyAmISeeingThisViewController {
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  // Create a label for the navigation bar title to allow multitline.
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.text =
      l10n_util::GetNSString(IDS_SEARCH_ENGINE_CHOICE_INFO_DIALOG_TITLE);
  titleLabel.numberOfLines = 0;
  titleLabel.adjustsFontForContentSizeCategory = YES;
  [titleLabel setTextAlignment:NSTextAlignmentCenter];
  titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  self.navigationItem.titleView = titleLabel;

  UIBarButtonItem* doneButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(doneButtonAction:)];
  self.navigationItem.rightBarButtonItem = doneButton;
  [self loadModel];
}

#pragma mark - Actions

- (void)doneButtonAction:(id)sender {
  [self.delegate learnMoreDone:self];
}

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:kSectionIdentifierEnumZero];

  NSString* paragraph1 = [l10n_util::GetNSString(
      IDS_SEARCH_ENGINE_CHOICE_INFO_DIALOG_BODY_FIRST_PARAGRAPH)
      stringByAppendingString:kEmptyLine];
  NSString* paragraph2 = [l10n_util::GetNSString(
      IDS_SEARCH_ENGINE_CHOICE_INFO_DIALOG_BODY_SECOND_PARAGRAPH)
      stringByAppendingString:kEmptyLine];
  NSString* paragraphs2and3 = [paragraph2
      stringByAppendingString:
          l10n_util::GetNSString(
              IDS_SEARCH_ENGINE_CHOICE_INFO_DIALOG_BODY_THIRD_PARAGRAPH)];

  NSString* infoString = [paragraph1 stringByAppendingString:paragraphs2and3];

  NSAttributedString* attributedString = PutBoldPartInText(infoString);

  TableViewAttributedStringHeaderFooterItem* footerItem =
      [[TableViewAttributedStringHeaderFooterItem alloc] init];
  footerItem.attributedString = attributedString;
  [model setFooter:footerItem
      forSectionWithIdentifier:kSectionIdentifierEnumZero];
}

@end
