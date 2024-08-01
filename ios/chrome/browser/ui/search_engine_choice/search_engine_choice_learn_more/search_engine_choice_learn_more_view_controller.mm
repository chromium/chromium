// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_learn_more/search_engine_choice_learn_more_view_controller.h"

#import <UIKit/UIKit.h>

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_constants.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/promo_style/utils.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/text_view_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Empty line between two paragraphs.
NSString* const kEmptyLine = @"\n\n";
NSString* const kBeginBoldTag = @"<b>";
NSString* const kEndBoldTag = @"</b>";

void AddBoldAttributeToString(NSMutableAttributedString* attributed_string,
                              NSRange range) {
  UIFontDescriptor* bold_descriptor = [[UIFontDescriptor
      preferredFontDescriptorWithTextStyle:UIFontTextStyleFootnote]
      fontDescriptorWithSymbolicTraits:UIFontDescriptorTraitBold];
  [attributed_string addAttribute:NSFontAttributeName
                            value:[UIFont fontWithDescriptor:bold_descriptor
                                                        size:0.0]
                            range:range];
  [attributed_string
      addAttribute:NSForegroundColorAttributeName
             value:[UIColor colorNamed:kSolidBlackColor]
             range:NSMakeRange(0, attributed_string.string.length)];
}

// The logic here is taken from PutBoldPartInString in
// "ios/chrome/common/string_util.h" except that we use desktop-style
// beginning / and end bold tags.
NSAttributedString* PutBoldPartInText(NSString* string) {
  UIFontDescriptor* default_descriptor = [UIFontDescriptor
      preferredFontDescriptorWithTextStyle:UIFontTextStyleFootnote];
  StringWithTag parsed_string =
      ParseStringWithTag(string, kBeginBoldTag, kEndBoldTag);

  NSMutableAttributedString* attributed_string =
      [[NSMutableAttributedString alloc] initWithString:parsed_string.string];
  [attributed_string addAttribute:NSFontAttributeName
                            value:[UIFont fontWithDescriptor:default_descriptor
                                                        size:0.0]
                            range:NSMakeRange(0, parsed_string.string.length)];

  AddBoldAttributeToString(attributed_string, parsed_string.range);

  [attributed_string addAttribute:NSForegroundColorAttributeName
                            value:[UIColor colorNamed:kSolidBlackColor]
                            range:NSMakeRange(0, parsed_string.string.length)];

  return attributed_string;
}

// Returns the first paragraph text view
UITextView* FirstParagraph() {
  UITextView* firstParagraphTextView = CreateUITextViewWithTextKit1();
  firstParagraphTextView.scrollEnabled = NO;
  firstParagraphTextView.editable = NO;
  firstParagraphTextView.backgroundColor = UIColor.clearColor;
  firstParagraphTextView.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  firstParagraphTextView.adjustsFontForContentSizeCategory = YES;
  firstParagraphTextView.translatesAutoresizingMaskIntoConstraints = NO;
  firstParagraphTextView.textContainerInset = UIEdgeInsetsMake(0, 0, 0, 0);
  firstParagraphTextView.showsVerticalScrollIndicator = NO;
  firstParagraphTextView.showsHorizontalScrollIndicator = NO;

  NSString* firstLine =
      [l10n_util::GetNSString(IDS_SEARCH_ENGINE_CHOICE_INFO_DIALOG_INTRO_IOS)
          stringByAppendingString:kEmptyLine];

  NSMutableAttributedString* firstParagraphAttributedString =
      [[NSMutableAttributedString alloc] initWithString:firstLine];
  AddBoldAttributeToString(firstParagraphAttributedString,
                           NSMakeRange(0, firstLine.length));
  NSString* paragraph1 = l10n_util::GetNSString(
      IDS_SEARCH_ENGINE_CHOICE_INFO_DIALOG_BODY_FIRST_PARAGRAPH);
  [firstParagraphAttributedString
      appendAttributedString:PutBoldPartInText(paragraph1)];

  firstParagraphTextView.attributedText = firstParagraphAttributedString;
  return firstParagraphTextView;
}

// Returns the second paragraph text view.
UITextView* SecondParagraph() {
  UITextView* secondParagraphTextView = CreateUITextViewWithTextKit1();
  secondParagraphTextView.scrollEnabled = NO;
  secondParagraphTextView.editable = NO;
  secondParagraphTextView.backgroundColor = UIColor.clearColor;
  secondParagraphTextView.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  secondParagraphTextView.adjustsFontForContentSizeCategory = YES;
  secondParagraphTextView.translatesAutoresizingMaskIntoConstraints = NO;
  secondParagraphTextView.textContainerInset = UIEdgeInsetsMake(0, 0, 0, 0);
  secondParagraphTextView.showsVerticalScrollIndicator = NO;
  secondParagraphTextView.showsHorizontalScrollIndicator = NO;

  NSString* paragraph2 = [l10n_util::GetNSString(
      IDS_SEARCH_ENGINE_CHOICE_INFO_DIALOG_BODY_SECOND_PARAGRAPH)
      stringByAppendingString:kEmptyLine];
  NSString* paragraphs2and3 = [paragraph2
      stringByAppendingString:
          l10n_util::GetNSString(
              IDS_SEARCH_ENGINE_CHOICE_INFO_DIALOG_BODY_THIRD_PARAGRAPH)];

  secondParagraphTextView.attributedText = PutBoldPartInText(paragraphs2and3);
  return secondParagraphTextView;
}

}  // namespace

@interface SearchEngineChoiceLearnMoreViewController () <UITextViewDelegate>
@end

@implementation SearchEngineChoiceLearnMoreViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  UIView* view = self.view;
  view.accessibilityIdentifier =
      kSearchEngineChoiceLearnMoreAccessibilityIdentifier;

  // Navigation.
  view.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
  self.navigationItem.title =
      l10n_util::GetNSString(IDS_SEARCH_ENGINE_CHOICE_INFO_DIALOG_TITLE_IOS);
  UIBarButtonItem* doneButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(doneButtonAction:)];
  self.navigationItem.rightBarButtonItem = doneButton;

  // Scroll view.
  UIScrollView* scrollView = [[UIScrollView alloc] init];
  [view addSubview:scrollView];
  scrollView.translatesAutoresizingMaskIntoConstraints = NO;

  // Scrollable content.
  UIView* scrollContentView = [[UIView alloc] init];
  [scrollView addSubview:scrollContentView];
  scrollContentView.translatesAutoresizingMaskIntoConstraints = NO;

  // First Paragraph.
  UITextView* firstParagraphTextView = FirstParagraph();
  firstParagraphTextView.delegate = self;
  [scrollContentView addSubview:firstParagraphTextView];

  // Rectangle with white background containing the image.
  UIView* imageViewContainer = [[UIView alloc] init];
  imageViewContainer.layer.cornerRadius = 12;
  imageViewContainer.translatesAutoresizingMaskIntoConstraints = NO;
  imageViewContainer.backgroundColor =
      [UIColor colorNamed:kPrimaryBackgroundColor];
  [scrollContentView addSubview:imageViewContainer];

  // Image.
  NSString* imageName = (self.traitCollection.horizontalSizeClass ==
                         UIUserInterfaceSizeClassCompact)
                            ? @"search_bar_iphone"
                            : @"search_bar_ipad";
  UIImage* image = [UIImage imageNamed:imageName];

  UIImageView* imageView = [[UIImageView alloc] initWithImage:image];
  imageView.accessibilityTraits = UIAccessibilityTraitNone;
  imageView.translatesAutoresizingMaskIntoConstraints = NO;
  [imageViewContainer addSubview:imageView];

  // Second paragraph.
  UITextView* secondParagraphTextView = SecondParagraph();
  secondParagraphTextView.delegate = self;
  [scrollContentView addSubview:secondParagraphTextView];

  // Create a layout guide to constrain the width of the content, while still
  // allowing the scroll view to take the full screen width.
  UILayoutGuide* widthLayoutGuide = AddPromoStyleWidthLayoutGuide(view);
  [NSLayoutConstraint activateConstraints:@[
    // Frame layout.
    [scrollView.frameLayoutGuide.topAnchor
        constraintEqualToAnchor:view.topAnchor],
    [scrollView.frameLayoutGuide.bottomAnchor
        constraintEqualToAnchor:view.bottomAnchor],
    [scrollView.frameLayoutGuide.leadingAnchor
        constraintEqualToAnchor:view.leadingAnchor],
    [scrollView.frameLayoutGuide.trailingAnchor
        constraintEqualToAnchor:view.trailingAnchor],

    // Content layout.
    [scrollView.contentLayoutGuide.trailingAnchor
        constraintEqualToAnchor:scrollView.frameLayoutGuide.trailingAnchor],
    [scrollView.contentLayoutGuide.leadingAnchor
        constraintEqualToAnchor:scrollView.frameLayoutGuide.leadingAnchor],

    // Scroll content view.
    [scrollContentView.topAnchor
        constraintEqualToAnchor:scrollView.contentLayoutGuide.topAnchor
                       constant:kTableViewVerticalSpacing],
    [scrollContentView.bottomAnchor
        constraintEqualToAnchor:scrollView.contentLayoutGuide.bottomAnchor
                       constant:-kTableViewVerticalSpacing],
    [scrollContentView.widthAnchor
        constraintEqualToAnchor:widthLayoutGuide.widthAnchor],
    [scrollContentView.centerXAnchor
        constraintEqualToAnchor:widthLayoutGuide.centerXAnchor],

    // first paragraph.
    [firstParagraphTextView.topAnchor
        constraintEqualToAnchor:scrollContentView.topAnchor],
    [firstParagraphTextView.leadingAnchor
        constraintEqualToAnchor:scrollContentView.leadingAnchor],
    [firstParagraphTextView.trailingAnchor
        constraintEqualToAnchor:scrollContentView.trailingAnchor],

    // Image view container.
    [imageViewContainer.topAnchor
        constraintEqualToAnchor:firstParagraphTextView.bottomAnchor
                       constant:kTableViewVerticalSpacing],
    [imageViewContainer.leadingAnchor
        constraintEqualToAnchor:scrollContentView.leadingAnchor],
    [imageViewContainer.trailingAnchor
        constraintEqualToAnchor:scrollContentView.trailingAnchor],

    // Image view.
    [imageView.topAnchor constraintEqualToAnchor:imageViewContainer.topAnchor],
    [imageView.bottomAnchor
        constraintEqualToAnchor:imageViewContainer.bottomAnchor],
    [imageView.centerXAnchor
        constraintEqualToAnchor:imageViewContainer.centerXAnchor],

    // second paragraph.
    [secondParagraphTextView.topAnchor
        constraintEqualToAnchor:imageViewContainer.bottomAnchor
                       constant:kTableViewVerticalSpacing],
    [secondParagraphTextView.leadingAnchor
        constraintEqualToAnchor:scrollContentView.leadingAnchor],
    [secondParagraphTextView.trailingAnchor
        constraintEqualToAnchor:scrollContentView.trailingAnchor],
    [secondParagraphTextView.bottomAnchor
        constraintEqualToAnchor:scrollContentView.bottomAnchor],

  ]];
}

#pragma mark - UITextViewDelegate

- (void)textViewDidChangeSelection:(UITextView*)textView {
  // Always force the `selectedTextRange` to `nil` to prevent users from
  // selecting text. Setting the `selectable` property to `NO` doesn't help
  // since it makes links inside the text view untappable.
  textView.selectedTextRange = nil;
}

#pragma mark - Actions

- (void)doneButtonAction:(id)sender {
  [self.delegate learnMoreDone:self];
}

#pragma mark - UIResponder

// To always be able to register key commands via -keyCommands, the VC must be
// able to become first responder.
- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (NSArray<UIKeyCommand*>*)keyCommands {
  return @[ UIKeyCommand.cr_close ];
}

- (void)keyCommand_close {
  base::RecordAction(base::UserMetricsAction("MobileKeyCommandClose"));
  [self.delegate learnMoreDone:self];
}

@end
