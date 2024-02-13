// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/elements/instruction_view.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/ui/elements/elements_constants.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/dynamic_type_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"

namespace {

constexpr CGFloat kStepNumberLabelSize = 20;
constexpr CGFloat kLeadingMargin = 15;
constexpr CGFloat kSpacing = 14;
constexpr CGFloat kVerticalMargin = 9;
constexpr CGFloat kTrailingMargin = 16;
constexpr CGFloat kCornerRadius = 12;
constexpr CGFloat kSeparatorLeadingMargin = 60;
constexpr CGFloat kSeparatorHeight = 0.5;
constexpr CGFloat kIconLabelWidth = 30;
// Height minimum for a line.
constexpr CGFloat kMinimumLineHeight = 44;

// Creates a view with `icon` in it.
UIView* CreateIconView(UIImage* icon) {
  UIImageView* icon_image_view = [[UIImageView alloc] initWithImage:icon];
  icon_image_view.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [icon_image_view.widthAnchor constraintEqualToConstant:kIconLabelWidth],
    [icon_image_view.heightAnchor constraintEqualToConstant:kIconLabelWidth],
  ]];
  return icon_image_view;
}

}  // namespace

@interface InstructionView ()

// The style of the instruction view.
@property(nonatomic, assign) InstructionViewStyle style;

// A list of step number labels for color reset on trait collection change.
@property(nonatomic, strong) NSMutableArray<UILabel*>* stepNumberLabels;

@end

@implementation InstructionView

#pragma mark - Public

- (instancetype)initWithList:(NSArray<NSString*>*)instructionList
                       style:(InstructionViewStyle)style
                   iconViews:(NSArray<UIView*>*)iconViews {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    BOOL useIcon = iconViews != nil;
    if (useIcon) {
      DCHECK(iconViews.count == instructionList.count);
    }

    _style = style;
    _stepNumberLabels =
        [[NSMutableArray alloc] initWithCapacity:instructionList.count];

    UIStackView* stackView = [[UIStackView alloc] init];
    stackView.translatesAutoresizingMaskIntoConstraints = NO;
    stackView.axis = UILayoutConstraintAxisVertical;
    UIView* firstBulletPoint =
        useIcon ? iconViews[0] : [self createStepNumberView:1];
    firstBulletPoint.translatesAutoresizingMaskIntoConstraints = NO;
    [stackView addArrangedSubview:[self createLineInstruction:instructionList[0]
                                              bulletPointView:firstBulletPoint
                                                        index:0]];
    for (NSUInteger i = 1; i < [instructionList count]; i++) {
      UIView* bulletPoint =
          useIcon ? iconViews[i] : [self createStepNumberView:i + 1];
      bulletPoint.translatesAutoresizingMaskIntoConstraints = NO;
      [stackView addArrangedSubview:[self createLineSeparator]];
      [stackView
          addArrangedSubview:[self createLineInstruction:instructionList[i]
                                         bulletPointView:bulletPoint
                                                   index:i]];
    }
    [self addSubview:stackView];
    AddSameConstraints(self, stackView);
    switch (style) {
      case InstructionViewStyleGrayscale:
        self.backgroundColor =
            [UIColor colorNamed:kGroupedSecondaryBackgroundColor];
        break;
      case InstructionViewStyleDefault:
        self.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
        break;
    }
    self.layer.cornerRadius = kCornerRadius;
  }
  return self;
}

- (instancetype)initWithList:(NSArray<NSString*>*)instructionList
                       style:(InstructionViewStyle)style
                       icons:(NSArray<UIImage*>*)icons {
  NSMutableArray<UIView*>* iconViews = nil;
  if (icons) {
    iconViews = [NSMutableArray array];
    for (UIImage* icon in icons) {
      UIView* iconView = CreateIconView(icon);
      [iconViews addObject:iconView];
    }
  }
  return [self initWithList:instructionList style:style iconViews:iconViews];
}

- (instancetype)initWithList:(NSArray<NSString*>*)instructionList
                       style:(InstructionViewStyle)style {
  return [self initWithList:instructionList style:style icons:nil];
}

- (instancetype)initWithList:(NSArray<NSString*>*)instructionList {
  return [self initWithList:instructionList style:InstructionViewStyleDefault];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (self.traitCollection.userInterfaceStyle !=
      previousTraitCollection.userInterfaceStyle) {
    for (UILabel* stepNumberLabel in self.stepNumberLabels) {
      [self updateColorForStepNumberLabel:stepNumberLabel];
    }
  }
}

#pragma mark - Private

// Creates a separator line.
- (UIView*)createLineSeparator {
  UIView* liner = [[UIView alloc] init];
  UIView* separator = [[UIView alloc] init];
  separator.backgroundColor = [UIColor colorNamed:kGrey300Color];
  separator.translatesAutoresizingMaskIntoConstraints = NO;

  [liner addSubview:separator];

  [NSLayoutConstraint activateConstraints:@[
    [separator.leadingAnchor constraintEqualToAnchor:liner.leadingAnchor
                                            constant:kSeparatorLeadingMargin],
    [separator.trailingAnchor constraintEqualToAnchor:liner.trailingAnchor],
    [separator.topAnchor constraintEqualToAnchor:liner.topAnchor],
    [separator.bottomAnchor constraintEqualToAnchor:liner.bottomAnchor],
    [liner.heightAnchor
        constraintEqualToConstant:AlignValueToPixel(kSeparatorHeight)],
  ]];

  return liner;
}

// Creates an instruction line with a bullet point view followed by
// instructions.
- (UIView*)createLineInstruction:(NSString*)instruction
                 bulletPointView:(UIView*)bulletPointView
                           index:(NSInteger)index {
  UILabel* instructionLabel = [[UILabel alloc] init];
  instructionLabel.textColor = [UIColor colorNamed:kGrey800Color];
  instructionLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];

  instructionLabel.attributedText =
      PutBoldPartInString(instruction, UIFontTextStyleSubheadline);
  instructionLabel.numberOfLines = 0;
  instructionLabel.adjustsFontForContentSizeCategory = YES;
  instructionLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [instructionLabel
      setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh + 1
                                      forAxis:UILayoutConstraintAxisVertical];

  UIView* line = [[UIView alloc] init];
  [line addSubview:bulletPointView];
  [line addSubview:instructionLabel];

  // Add constraints for bulletPointView and instructionLabel vertical margins
  // to make sure that they are as small as possible.
  NSLayoutConstraint* minimumBulletPointTopMargin =
      [bulletPointView.topAnchor constraintEqualToAnchor:line.topAnchor
                                                constant:kVerticalMargin];
  minimumBulletPointTopMargin.priority = UILayoutPriorityDefaultHigh;
  NSLayoutConstraint* minimumBulletPointBottomMargin =
      [bulletPointView.bottomAnchor constraintEqualToAnchor:line.bottomAnchor
                                                   constant:-kVerticalMargin];
  minimumBulletPointBottomMargin.priority = UILayoutPriorityDefaultHigh;
  NSLayoutConstraint* minimumLabelTopMargin =
      [instructionLabel.topAnchor constraintEqualToAnchor:line.topAnchor
                                                 constant:kVerticalMargin];
  minimumLabelTopMargin.priority = UILayoutPriorityDefaultHigh;
  NSLayoutConstraint* minimumLabelBottomMargin =
      [instructionLabel.bottomAnchor constraintEqualToAnchor:line.bottomAnchor
                                                    constant:-kVerticalMargin];
  minimumLabelBottomMargin.priority = UILayoutPriorityDefaultHigh;
  [NSLayoutConstraint activateConstraints:@[
    [line.heightAnchor
        constraintGreaterThanOrEqualToConstant:kMinimumLineHeight],
    [bulletPointView.leadingAnchor constraintEqualToAnchor:line.leadingAnchor
                                                  constant:kLeadingMargin],
    [bulletPointView.centerYAnchor constraintEqualToAnchor:line.centerYAnchor],
    [instructionLabel.leadingAnchor
        constraintEqualToAnchor:bulletPointView.trailingAnchor
                       constant:kSpacing],
    [instructionLabel.centerYAnchor constraintEqualToAnchor:line.centerYAnchor],
    minimumBulletPointTopMargin, minimumBulletPointBottomMargin,
    minimumLabelTopMargin, minimumLabelBottomMargin,
    [bulletPointView.bottomAnchor
        constraintLessThanOrEqualToAnchor:line.bottomAnchor
                                 constant:-kVerticalMargin],
    [bulletPointView.topAnchor
        constraintGreaterThanOrEqualToAnchor:line.topAnchor
                                    constant:kVerticalMargin],
    [instructionLabel.bottomAnchor
        constraintLessThanOrEqualToAnchor:line.bottomAnchor
                                 constant:-kVerticalMargin],
    [instructionLabel.topAnchor
        constraintGreaterThanOrEqualToAnchor:line.topAnchor
                                    constant:kVerticalMargin],
    [instructionLabel.trailingAnchor constraintEqualToAnchor:line.trailingAnchor
                                                    constant:-kTrailingMargin]
  ]];

  line.tag = index;
  line.accessibilityIdentifier =
      InstructionViewRowAccessibilityIdentifier(index);
  line.accessibilityElements = @[ bulletPointView, instructionLabel ];
  // Don't set the accessibility traits indicating that it is tappable as we do
  // not actually expect any action, instead, we just want to measure how many
  // people believe it’s tappable.
  return line;
}

// Creates a view with a round numbered label in it.
- (UIView*)createStepNumberView:(NSInteger)stepNumber {
  UILabel* stepNumberLabel = [[UILabel alloc] initWithFrame:CGRectZero];
  stepNumberLabel.translatesAutoresizingMaskIntoConstraints = NO;
  stepNumberLabel.textAlignment = NSTextAlignmentCenter;
  stepNumberLabel.text = [@(stepNumber) stringValue];
  stepNumberLabel.font = PreferredFontForTextStyleWithMaxCategory(
      UIFontTextStyleFootnote,
      self.traitCollection.preferredContentSizeCategory,
      UIContentSizeCategoryExtraExtraExtraLarge);

  UIFontDescriptor* boldFontDescriptor = [stepNumberLabel.font.fontDescriptor
      fontDescriptorWithSymbolicTraits:UIFontDescriptorTraitBold];
  stepNumberLabel.font = [UIFont fontWithDescriptor:boldFontDescriptor size:0];

  stepNumberLabel.layer.cornerRadius = kStepNumberLabelSize / 2;
  [self updateColorForStepNumberLabel:stepNumberLabel];
  [self.stepNumberLabels addObject:stepNumberLabel];

  UIView* labelContainer = [[UIView alloc] initWithFrame:CGRectZero];
  labelContainer.translatesAutoresizingMaskIntoConstraints = NO;

  [labelContainer addSubview:stepNumberLabel];

  [NSLayoutConstraint activateConstraints:@[
    [stepNumberLabel.centerYAnchor
        constraintEqualToAnchor:labelContainer.centerYAnchor],
    [stepNumberLabel.centerXAnchor
        constraintEqualToAnchor:labelContainer.centerXAnchor],
    [stepNumberLabel.widthAnchor
        constraintEqualToConstant:kStepNumberLabelSize],
    [stepNumberLabel.heightAnchor
        constraintEqualToConstant:kStepNumberLabelSize],

    [labelContainer.widthAnchor constraintEqualToConstant:kIconLabelWidth],
    [labelContainer.heightAnchor
        constraintEqualToAnchor:stepNumberLabel.heightAnchor],
  ]];

  return labelContainer;
}

// Sets and update the background color of the step number label on
// initialization and when entering or exiting dark mode.
- (void)updateColorForStepNumberLabel:(UILabel*)stepNumberLabel {
  switch (self.style) {
    case InstructionViewStyleGrayscale:
      stepNumberLabel.textColor = [UIColor colorNamed:kGrey600Color];
      stepNumberLabel.layer.backgroundColor =
          [UIColor colorNamed:kGroupedPrimaryBackgroundColor].CGColor;
      break;
    case InstructionViewStyleDefault:
      stepNumberLabel.textColor = [UIColor colorNamed:kBlueColor];
      stepNumberLabel.layer.backgroundColor =
          [UIColor colorNamed:kPrimaryBackgroundColor].CGColor;
      break;
  }
}

@end
