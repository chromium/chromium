// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_inline_notice_view.h"

#import "base/apple/foundation_util.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Spacing between the text views in the vertical stack.
constexpr CGFloat kTextStackSpacing = 4.0;

// Spacing between the text stack and the OK button.
constexpr CGFloat kMainStackSpacing = 16.0;

// Margin constants.
constexpr CGFloat kMarginTop = 12.0;
constexpr CGFloat kMarginLeading = 16.0;
constexpr CGFloat kMarginBottom = 12.0;
constexpr CGFloat kMarginTrailing = 16.0;

}  // namespace

@interface AtMemoryInlineNoticeView () <UITextViewDelegate>
@end

@implementation AtMemoryInlineNoticeConfiguration

- (instancetype)init {
  self = [super init];
  if (self) {
    _title = l10n_util::GetNSString(IDS_AT_MEMORY_NOTICE_TITLE);
    _message = l10n_util::GetNSString(IDS_AT_MEMORY_NOTICE_TEXT_NO_LOGGING);
  }
  return self;
}

- (id<UIContentView>)makeContentView {
  return [[AtMemoryInlineNoticeView alloc] initWithConfiguration:self];
}

- (instancetype)updatedConfigurationForState:(id<UIConfigurationState>)state {
  return self;
}

- (id)copyWithZone:(NSZone*)zone {
  AtMemoryInlineNoticeConfiguration* copy =
      [[[self class] allocWithZone:zone] init];
  copy.title = self.title;
  copy.message = self.message;
  copy.delegate = self.delegate;
  return copy;
}

@end

#pragma mark - AtMemoryInlineNoticeView

@implementation AtMemoryInlineNoticeView {
  // The configuration for this view.
  AtMemoryInlineNoticeConfiguration* _configuration;
  // The main layout stack.
  UIStackView* _mainStack;
  // Label for the notice title header.
  UILabel* _titleLabel;
  // Text view that displays the notice description and manages link taps.
  UITextView* _subtitleTextView;
  // Button that acknowledges the notice when tapped.
  UIButton* _okButton;
}

- (instancetype)initWithConfiguration:
    (AtMemoryInlineNoticeConfiguration*)configuration {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _titleLabel = [self createTitleLabel];
    _subtitleTextView = [self createSubtitleTextView];
    _okButton = [self createOKButton];

    UIStackView* textStack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _titleLabel, _subtitleTextView ]];
    textStack.axis = UILayoutConstraintAxisVertical;
    textStack.spacing = kTextStackSpacing;
    textStack.translatesAutoresizingMaskIntoConstraints = NO;

    _mainStack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ textStack, _okButton ]];
    _mainStack.axis = UILayoutConstraintAxisHorizontal;
    _mainStack.spacing = kMainStackSpacing;
    _mainStack.alignment = UIStackViewAlignmentCenter;
    _mainStack.layoutMargins = UIEdgeInsetsMake(kMarginTop, kMarginLeading,
                                                kMarginBottom, kMarginTrailing);
    _mainStack.layoutMarginsRelativeArrangement = YES;
    _mainStack.translatesAutoresizingMaskIntoConstraints = NO;

    [self addSubview:_mainStack];
    AddSameConstraints(_mainStack, self);

    [self applyContentSizeCategoryStyles];

    [self registerForTraitChanges:@[ UITraitPreferredContentSizeCategory.class ]
                       withAction:@selector(applyContentSizeCategoryStyles)];

    self.configuration = configuration;
  }
  return self;
}

#pragma mark - UIContentView

- (id<UIContentConfiguration>)configuration {
  return _configuration;
}

- (void)setConfiguration:(id<UIContentConfiguration>)configuration {
  if (![self supportsConfiguration:configuration]) {
    return;
  }
  AtMemoryInlineNoticeConfiguration* config =
      base::apple::ObjCCast<AtMemoryInlineNoticeConfiguration>(configuration);
  _configuration = [config copy];

  _titleLabel.text = _configuration.title;

  NSDictionary* textAttributes = @{
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote],
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor]
  };
  NSDictionary* linkAttributes = @{
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote],
    NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor],
    NSLinkAttributeName : @"",
  };
  NSString* message = _configuration.message;
  // Convert HTML link tags to BEGIN_LINK/END_LINK delimiters used on iOS.
  message = [message stringByReplacingOccurrencesOfString:@"<link>"
                                               withString:@"BEGIN_LINK"];
  message = [message stringByReplacingOccurrencesOfString:@"</link>"
                                               withString:@"END_LINK"];

  _subtitleTextView.attributedText = AttributedStringFromStringWithLink(
      message, textAttributes, linkAttributes);
}

- (BOOL)supportsConfiguration:(id<UIContentConfiguration>)configuration {
  return
      [configuration isKindOfClass:[AtMemoryInlineNoticeConfiguration class]];
}

#pragma mark - UITextViewDelegate

- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange {
  [_configuration.delegate inlineNoticeViewDidTapSettings:self];
  return NO;
}

- (void)textViewDidChangeSelection:(UITextView*)textView {
  textView.selectedTextRange = nil;
}

// Adjusts stack view alignments for accessibility sizes.
- (void)applyContentSizeCategoryStyles {
  if (UIContentSizeCategoryIsAccessibilityCategory(
          self.traitCollection.preferredContentSizeCategory)) {
    _mainStack.axis = UILayoutConstraintAxisVertical;
    _mainStack.alignment = UIStackViewAlignmentFill;
  } else {
    _mainStack.axis = UILayoutConstraintAxisHorizontal;
    _mainStack.alignment = UIStackViewAlignmentCenter;
  }
}

#pragma mark - Private

// Action triggered by the OK button.
- (void)didTapOK {
  [_configuration.delegate inlineNoticeViewDidTapOK:self];
}

// Helper to create and configure the title label.
- (UILabel*)createTitleLabel {
  UILabel* label = [[UILabel alloc] init];
  label.numberOfLines = 0;
  label.textColor = [UIColor colorNamed:kTextPrimaryColor];
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  label.adjustsFontForContentSizeCategory = YES;
  label.translatesAutoresizingMaskIntoConstraints = NO;
  return label;
}

// Helper to create and configure the subtitle text view.
- (UITextView*)createSubtitleTextView {
  UITextView* textView = [[UITextView alloc] init];
  textView.scrollEnabled = NO;
  textView.editable = NO;
  textView.selectable = YES;
  textView.backgroundColor = [UIColor clearColor];
  textView.textContainer.lineFragmentPadding = 0;
  textView.textContainerInset = UIEdgeInsetsZero;
  textView.delegate = self;
  textView.adjustsFontForContentSizeCategory = YES;
  textView.translatesAutoresizingMaskIntoConstraints = NO;
  return textView;
}

// Helper to create and configure the OK button.
- (UIButton*)createOKButton {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem];
  [button setTitle:l10n_util::GetNSString(IDS_OK)
          forState:UIControlStateNormal];
  [button.titleLabel
      setFont:[UIFont preferredFontForTextStyle:UIFontTextStyleHeadline]];
  button.titleLabel.adjustsFontForContentSizeCategory = YES;
  [button setTitleColor:[UIColor colorNamed:kBlueColor]
               forState:UIControlStateNormal];
  [button addTarget:self
                action:@selector(didTapOK)
      forControlEvents:UIControlEventTouchUpInside];
  button.translatesAutoresizingMaskIntoConstraints = NO;

  [button
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [button setContentHuggingPriority:UILayoutPriorityRequired
                            forAxis:UILayoutConstraintAxisHorizontal];
  return button;
}

@end
