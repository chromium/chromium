// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_consent_accordion_view.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Stack view attributes.
const CGFloat kRowSpacing = 2.0;
const CGFloat kCornerRadius = 16.0;
const CGFloat kSideContainerSize = 48.0;

// Inner stack view spacing and padding.
const CGFloat kStackViewSpacing = 6.0;
const CGFloat kStackViewPadding = 12.0;

// Side accessory views attributes.
const CGFloat kAccessoryTopPadding = 18.0;

// Chevron attributes.
const CGFloat kChevronSize = 16.0;

// Animation attributes.
const NSTimeInterval kAnimationDuration = 0.3;

}  // namespace

#pragma mark - GeminiConsentAccordionView

// Private methods called by GeminiConsentRowView to delegate actions.
@interface GeminiConsentAccordionView ()

// Handle url taps in the body.
- (void)didTapLink:(NSURL*)url;
// Handle row tap that toggles between collapsed/expanded states.
- (void)didToggleRow:(GeminiConsentRow*)row;

@end

#pragma mark - GeminiConsentRow

@implementation GeminiConsentRow

// Initializes the row data model, defaulting to a collapsed state.
- (instancetype)initWithIcon:(UIImage*)icon
                       title:(NSString*)title
                        body:(NSAttributedString*)body {
  self = [super init];
  if (self) {
    _icon = icon;
    _title = [title copy];
    _body = [body copy];
    _collapsed = YES;
  }
  return self;
}

@end

#pragma mark - GeminiConsentRowView

// Row in the accordion. See `gemini_consent_accordion_view.h` for the layout.
@interface GeminiConsentRowView : UIView <UITextViewDelegate>

// Initializes the views and optionally add a tap gesture when collapsible.
- (instancetype)initWithRow:(GeminiConsentRow*)row
                collapsible:(BOOL)collapsible
              accordionView:(GeminiConsentAccordionView*)accordionView
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// Toggles the row view's expand/collapse state and triggers animations.
- (void)toggleState;

@end

@implementation GeminiConsentRowView {
  // Title view that sits above the body.
  __weak UILabel* _titleView;
  // Body view, hidden initially when the row is collapsible.
  __weak UITextView* _bodyView;
  // Optional chevron, present when the row is collapsible.
  __weak UIView* _chevronView;

  // View data for the row.
  GeminiConsentRow* _row;
  // Whether the row collapses/expands when tapped.
  BOOL _collapsible;
  // Weak reference to the parent view and its delegate.
  __weak GeminiConsentAccordionView* _accordionView;
}

// Initializes the view with a data model, collapsible setting, and parent view.
- (instancetype)initWithRow:(GeminiConsentRow*)row
                collapsible:(BOOL)collapsible
              accordionView:(GeminiConsentAccordionView*)accordionView {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    // Initialize collapsible row to collapsed, otherwise keep it expanded.
    row.collapsed = collapsible;
    _row = row;
    _collapsible = collapsible;
    _accordionView = accordionView;

    UIStackView* stackView = [[UIStackView alloc] init];
    stackView.alignment = UIStackViewAlignmentTop;
    stackView.translatesAutoresizingMaskIntoConstraints = NO;
    stackView.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
    [self addSubview:stackView];
    AddSameConstraints(stackView, self);

    // Icon.
    UIImageView* iconView = [[UIImageView alloc] initWithImage:row.icon];
    iconView.contentMode = UIViewContentModeScaleAspectFit;
    iconView.translatesAutoresizingMaskIntoConstraints = NO;
    [stackView addArrangedSubview:[self createContainerForView:iconView]];

    // Content (Title + Body).
    UIView* contentStack = [self createContentStackWithRow:row];
    [stackView addArrangedSubview:contentStack];

    // Chevron (Only if collapsible).
    if (collapsible) {
      UIImageSymbolConfiguration* config = [UIImageSymbolConfiguration
          configurationWithPointSize:kChevronSize
                              weight:UIImageSymbolWeightMedium];
      UIImageView* chevronView =
          [[UIImageView alloc] initWithImage:DefaultSymbolWithConfiguration(
                                                 kChevronDownSymbol, config)];
      chevronView.tintColor = [UIColor colorNamed:kTextSecondaryColor];
      chevronView.contentMode = UIViewContentModeScaleAspectFit;
      chevronView.translatesAutoresizingMaskIntoConstraints = NO;
      [stackView addArrangedSubview:[self createContainerForView:chevronView]];

      _chevronView = chevronView;

      // Interaction.
      UITapGestureRecognizer* tapGesture =
          [[UITapGestureRecognizer alloc] initWithTarget:self
                                                  action:@selector(didTapRow)];
      [stackView addGestureRecognizer:tapGesture];
      stackView.userInteractionEnabled = YES;
    }
  }
  return self;
}

// Toggles the row view's expand/collapse state and triggers animations.
- (void)toggleState {
  _row.collapsed = !_row.collapsed;
  _titleView.accessibilityValue =
      _row.collapsed
          ? l10n_util::GetNSString(IDS_IOS_GEMINI_ACCORDION_COLLAPSED)
          : l10n_util::GetNSString(IDS_IOS_GEMINI_ACCORDION_EXPANDED);
  _bodyView.hidden = _row.collapsed;

  [_accordionView didToggleRow:_row];

  __weak __typeof(self) weakSelf = self;
  [UIView animateWithDuration:kAnimationDuration
                        delay:0.0
       usingSpringWithDamping:0.7
        initialSpringVelocity:0.5
                      options:UIViewAnimationOptionCurveEaseInOut
                   animations:^{
                     [weakSelf animateToggleState];
                   }
                   completion:nil];
}

#pragma mark - Private

// Wraps the subview in a fixed-width, top-padded container.
- (UIView*)createContainerForView:(UIView*)subview {
  UIView* container = [[UIView alloc] init];
  container.translatesAutoresizingMaskIntoConstraints = NO;
  container.directionalLayoutMargins =
      NSDirectionalEdgeInsetsMake(kAccessoryTopPadding, 0, 0, 0);
  [container addSubview:subview];
  [container.widthAnchor constraintEqualToConstant:kSideContainerSize].active =
      YES;
  AddSameConstraints(subview, container.layoutMarginsGuide);
  return container;
}

// Creates the vertical UIStackView containing the title and body views.
- (UIStackView*)createContentStackWithRow:(GeminiConsentRow*)row {
  UIStackView* innerStackView = [[UIStackView alloc] init];
  innerStackView.axis = UILayoutConstraintAxisVertical;
  innerStackView.alignment = UIStackViewAlignmentFill;
  innerStackView.spacing = kStackViewSpacing;
  innerStackView.translatesAutoresizingMaskIntoConstraints = NO;

  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.text = row.title;
  titleLabel.font =
      PreferredFontForTextStyle(UIFontTextStyleHeadline, UIFontWeightSemibold);
  titleLabel.numberOfLines = 0;
  [titleLabel
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [innerStackView addArrangedSubview:titleLabel];

  if (_collapsible) {
    titleLabel.accessibilityTraits = UIAccessibilityTraitButton;
    titleLabel.accessibilityValue =
        row.collapsed
            ? l10n_util::GetNSString(IDS_IOS_GEMINI_ACCORDION_COLLAPSED)
            : l10n_util::GetNSString(IDS_IOS_GEMINI_ACCORDION_EXPANDED);
  }
  _titleView = titleLabel;

  UITextView* bodyTextView = [[UITextView alloc] init];
  bodyTextView.backgroundColor = [UIColor clearColor];
  bodyTextView.scrollEnabled = NO;
  bodyTextView.editable = NO;
  bodyTextView.textDragInteraction.enabled = NO;
  bodyTextView.textContainerInset = UIEdgeInsetsZero;
  bodyTextView.textContainer.lineFragmentPadding = 0;
  bodyTextView.font = PreferredFontForTextStyle(UIFontTextStyleBody);
  bodyTextView.textColor = [UIColor colorNamed:kTextSecondaryColor];
  bodyTextView.adjustsFontForContentSizeCategory = YES;
  bodyTextView.linkTextAttributes =
      @{NSForegroundColorAttributeName : [UIColor colorNamed:kBlue600Color]};
  bodyTextView.attributedText = row.body;
  bodyTextView.hidden = _collapsible;
  bodyTextView.delegate = self;
  [innerStackView addArrangedSubview:bodyTextView];

  _bodyView = bodyTextView;

  innerStackView.layoutMarginsRelativeArrangement = YES;
  innerStackView.layoutMargins = UIEdgeInsetsMake(
      kStackViewPadding, 0, kStackViewPadding, kStackViewPadding);

  return innerStackView;
}

// Helper method containing the layout animations for toggling.
- (void)animateToggleState {
  CGFloat angle = _row.collapsed ? 0 : M_PI;
  _chevronView.transform = CGAffineTransformMakeRotation(angle);
  [self layoutIfNeeded];
}

// Action triggered when the collapsible row view is tapped.
- (void)didTapRow {
  [self toggleState];
}

- (UIAction*)textView:(UITextView*)textView
    primaryActionForTextItem:(UITextItem*)textItem
               defaultAction:(UIAction*)defaultAction {
  if (!textItem.link) {
    return nil;
  }

  NSURL* url = textItem.link;
  __weak GeminiConsentAccordionView* weakAccordionView = _accordionView;
  return [UIAction actionWithHandler:^(UIAction* action) {
    [weakAccordionView didTapLink:url];
  }];
}

@end

#pragma mark - GeminiConsentAccordionView

@implementation GeminiConsentAccordionView {
  UIStackView* _containerStackView;
}

// Initializes the accordion view with a stack of consent row views.
- (instancetype)initWithRows:(NSArray<GeminiConsentRow*>*)rows
                 collapsible:(BOOL)collapsible {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _containerStackView = [[UIStackView alloc] init];
    _containerStackView.axis = UILayoutConstraintAxisVertical;
    _containerStackView.spacing = kRowSpacing;
    _containerStackView.layer.cornerRadius = kCornerRadius;
    _containerStackView.clipsToBounds = YES;
    _containerStackView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_containerStackView];
    AddSameConstraints(_containerStackView, self);

    for (GeminiConsentRow* row in rows) {
      GeminiConsentRowView* rowView =
          [[GeminiConsentRowView alloc] initWithRow:row
                                        collapsible:collapsible
                                      accordionView:self];
      [_containerStackView addArrangedSubview:rowView];
    }
  }
  return self;
}

#pragma mark - GeminiConsentAccordionViewDelegate

// Calls the delegate when an URL is tapped in the body.
- (void)didTapLink:(NSURL*)url {
  [self.delegate accordionView:self didTapLink:url];
}

// Calls the delegate when a row is toggled between collapsed/expanded states.
- (void)didToggleRow:(GeminiConsentRow*)row {
  [self.delegate accordionView:self didToggleRow:row];
}

@end
