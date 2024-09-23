
// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "ios/chrome/browser/ui/omnibox/popup/row/actions/actions_view.h"

#import "ios/chrome/browser/ui/omnibox/popup/row/actions/omnibox_popup_actions_row_content_configuration.h"
#import "ios/chrome/browser/ui/omnibox/popup/row/actions/omnibox_popup_actions_row_delegate.h"
#import "ios/chrome/browser/ui/omnibox/popup/row/actions/suggest_action.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
/// The scroll view height.
const CGFloat kOmniboxPopupActionsHeight = 44;
///  Space between buttons.
const CGFloat kButtonSpace = 8;
/// The padding between the button's image and title.
const CGFloat kButtonTitleImagePadding = 4;
/// The button's image size.
const CGFloat kIconSize = 13;
/// Actions leading padding.
const CGFloat kLeadingPadding = 61;
}  // namespace

@implementation ActionsView {
  UIScrollView* _actionsScrollView;
  UIStackView* _actionsStackView;
  OmniboxPopupActionsRowContentConfiguration* _configuration;
}

- (instancetype)initWithConfiguration:
    (OmniboxPopupActionsRowContentConfiguration*)configuration {
  self = [super init];
  if (self) {
    _configuration = configuration;

    _actionsScrollView = [[UIScrollView alloc] init];
    _actionsScrollView.translatesAutoresizingMaskIntoConstraints = NO;
    _actionsScrollView.showsHorizontalScrollIndicator = NO;

    _actionsStackView = [[UIStackView alloc] init];
    _actionsStackView.translatesAutoresizingMaskIntoConstraints = NO;
    _actionsStackView.alignment = UIStackViewAlignmentCenter;
    _actionsStackView.axis = UILayoutConstraintAxisHorizontal;
    _actionsStackView.spacing = kButtonSpace;

    [self setupActionsStackView:configuration];

    [_actionsScrollView addSubview:_actionsStackView];
    [self addSubview:_actionsScrollView];
    AddSameConstraints(self, _actionsScrollView);
    AddSameConstraintsWithInsets(
        _actionsStackView, _actionsScrollView.contentLayoutGuide,
        NSDirectionalEdgeInsetsMake(0, kLeadingPadding, 0, 0));

    [NSLayoutConstraint activateConstraints:@[
      [self.heightAnchor
          constraintGreaterThanOrEqualToConstant:kOmniboxPopupActionsHeight],
      [_actionsScrollView.heightAnchor
          constraintEqualToAnchor:_actionsStackView.heightAnchor],
      // Constraint the stackview's width to be able to scroll to semantic
      // leading.
      [_actionsStackView.widthAnchor
          constraintGreaterThanOrEqualToAnchor:_actionsScrollView.widthAnchor
                                      constant:-kLeadingPadding]
    ]];
  }
  return self;
}

- (void)setSemanticContentAttribute:
    (UISemanticContentAttribute)semanticContentAttribute {
  [super setSemanticContentAttribute:semanticContentAttribute];

  // prevent unnecessary semantic update and scrolling.
  if (_actionsScrollView.semanticContentAttribute == semanticContentAttribute) {
    return;
  }

  _actionsScrollView.semanticContentAttribute = semanticContentAttribute;
  _actionsStackView.semanticContentAttribute = semanticContentAttribute;

  [_actionsScrollView layoutIfNeeded];

  BOOL isRTL = [UIView userInterfaceLayoutDirectionForSemanticContentAttribute:
                           semanticContentAttribute] ==
               UIUserInterfaceLayoutDirectionRightToLeft;

  CGFloat contentStartX = 0;

  if (isRTL) {
    contentStartX = MAX(_actionsScrollView.contentSize.width - 1, 0);
  }

  [_actionsScrollView scrollRectToVisible:CGRectMake(contentStartX, 0, 1, 1)
                                 animated:NO];
}

- (void)updateConfiguration:
    (OmniboxPopupActionsRowContentConfiguration*)configuration {
  _configuration = configuration;
  [self setupActionsStackView:configuration];
  [self scrollToHighlightedAction];
}

#pragma mark - Private

// Scrolls to the highlighted action, if there is any.
- (void)scrollToHighlightedAction {
  if (_configuration.highlightedActionIndex == NSNotFound) {
    return;
  }

  // Make sure the scroll view layout is done before computing frames.
  [_actionsScrollView layoutIfNeeded];

  NSUInteger highlightedActionIndex = _configuration.highlightedActionIndex;

  CHECK(_actionsStackView.arrangedSubviews.count > highlightedActionIndex);

  UIButton* highlightedActionButton =
      _actionsStackView.arrangedSubviews[highlightedActionIndex];

  CGRect frameInScrollViewCoordinates =
      [highlightedActionButton convertRect:highlightedActionButton.bounds
                                    toView:_actionsScrollView];
  CGRect frameWithPadding =
      CGRectInset(frameInScrollViewCoordinates, -kLeadingPadding, 0);
  [_actionsScrollView scrollRectToVisible:frameWithPadding animated:NO];
}

// Setup the actions buttons with the given configuration actions and adds them
// to the actions stack view.
- (void)setupActionsStackView:
    (OmniboxPopupActionsRowContentConfiguration*)configuration {
  // Clear out previous arranged buttons.
  for (UIView* view in _actionsStackView.arrangedSubviews) {
    [view removeFromSuperview];
  }

  for (NSUInteger i = 0; i < configuration.actions.count; i++) {
    SuggestAction* action = configuration.actions[i];

    BOOL isActionHighlighted = configuration.highlightedActionIndex == i;

    UIButton* actionButton =
        [[self class] actionButtonWithSuggestAction:action
                                        highlighted:isActionHighlighted];

    [_actionsStackView addArrangedSubview:actionButton];

    if (action.type == omnibox::ActionInfo_ActionType_CALL) {
      [actionButton addTarget:self
                       action:@selector(callButtonTapped)
             forControlEvents:UIControlEventTouchUpInside];
      actionButton.accessibilityLabel = l10n_util::GetNSString(
          IDS_IOS_CALL_OMNIBOX_ACTION_ACCESSIBILITY_LABEL);
    } else if (action.type == omnibox::ActionInfo_ActionType_DIRECTIONS) {
      [actionButton addTarget:self
                       action:@selector(directionsButtonTapped)
             forControlEvents:UIControlEventTouchUpInside];
      actionButton.accessibilityLabel = l10n_util::GetNSString(
          IDS_IOS_DIRECTIONS_OMNIBOX_ACTION_ACCESSIBILITY_LABEL);
    } else if (action.type == omnibox::ActionInfo_ActionType_REVIEWS) {
      [actionButton addTarget:self
                       action:@selector(reviewsButtonTapped)
             forControlEvents:UIControlEventTouchUpInside];
      actionButton.accessibilityLabel = l10n_util::GetNSString(
          IDS_IOS_REVIEWS_OMNIBOX_ACTION_ACCESSIBILITY_LABEL);
    }
  }

  // add a spacer to the stack view in order to fill the available stack view
  // width.
  UIView* spacerView = [[UIView alloc] init];
  spacerView.translatesAutoresizingMaskIntoConstraints = NO;
  [spacerView
      setContentCompressionResistancePriority:0
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [_actionsStackView addArrangedSubview:spacerView];
}

// Creates a suggestion action button and setup its configutation attributes
// (eg. Call action button).
+ (UIButton*)actionButtonWithSuggestAction:(SuggestAction*)suggestAction
                               highlighted:(BOOL)highlighted {
  UIButton* button = [[UIButton alloc] init];

  [button
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [button
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisVertical];

  button.accessibilityIdentifier =
      [suggestAction.class accessibilityIdentifierWithType:suggestAction.type
                                               highlighted:highlighted];

  NSAttributedString* attributedTitle = [[NSAttributedString alloc]
      initWithString:suggestAction.title
          attributes:@{
            NSFontAttributeName :
                [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote]
          }];

  UIButtonConfiguration* configuration =
      highlighted ? [UIButtonConfiguration filledButtonConfiguration]
                  : [UIButtonConfiguration plainButtonConfiguration];
  configuration.imagePadding = kButtonTitleImagePadding;
  configuration.attributedTitle = attributedTitle;
  configuration.image = [SuggestAction imageIconForAction:suggestAction
                                                     size:kIconSize];
  if (!highlighted) {
    UIBackgroundConfiguration* backgroundConfig =
        [UIBackgroundConfiguration clearConfiguration];
    backgroundConfig.backgroundColor =
        [UIColor colorNamed:kTertiaryBackgroundColor];
    configuration.background = backgroundConfig;
  }

  button.configuration = configuration;

  [button sizeToFit];
  button.layer.cornerRadius = button.frame.size.height / 2;
  button.layer.masksToBounds = YES;

  return button;
}

/// Handles tap on call button
- (void)callButtonTapped {
  SuggestAction* callAction =
      [self actionWithType:omnibox::ActionInfo_ActionType_CALL];
  [_configuration.delegate
      omniboxPopupRowActionSelectedWithConfiguration:_configuration
                                              action:callAction];
}

/// Handles tap on directions button
- (void)directionsButtonTapped {
  SuggestAction* directionsAction =
      [self actionWithType:omnibox::ActionInfo_ActionType_DIRECTIONS];
  [_configuration.delegate
      omniboxPopupRowActionSelectedWithConfiguration:_configuration
                                              action:directionsAction];
}

/// Handles tap on reviews button
- (void)reviewsButtonTapped {
  SuggestAction* reviewsAction =
      [self actionWithType:omnibox::ActionInfo_ActionType_REVIEWS];
  [_configuration.delegate
      omniboxPopupRowActionSelectedWithConfiguration:_configuration
                                              action:reviewsAction];
}

/// Returns the action that corresponds to the given type.
- (SuggestAction*)actionWithType:(omnibox::ActionInfo::ActionType)actionType {
  for (SuggestAction* action in _configuration.actions) {
    if (action.type == actionType) {
      return action;
    }
  }
  return nil;
}

@end
