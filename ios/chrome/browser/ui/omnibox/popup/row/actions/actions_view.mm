
// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "ios/chrome/browser/ui/omnibox/popup/row/actions/actions_view.h"

#import "ios/chrome/browser/ui/omnibox/popup/row/actions/omnibox_popup_actions_row_content_configuration.h"
#import "ios/chrome/browser/ui/omnibox/popup/row/actions/suggest_action.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
/// The scroll view height.
const CGFloat kOmniboxPopupActionsHeight = 44;
///  Space between buttons.
const CGFloat kButtonSpace = 5;
/// The padding between the button's image and title.
const CGFloat kButtonTitleImagePadding = 4;
/// The button's image size.
const CGFloat kIconSize = 13;
/// The button's layer radius.
const CGFloat kButtonLayerRadius = 12;
/// Actions leading and trailing padding.
const CGFloat kLeadingTrailingPadding = 61;
}  // namespace

@implementation ActionsView {
  UIScrollView* _actionsScrollView;
  UIStackView* _actionsStackView;
}

- (instancetype)initWithConfiguration:
    (OmniboxPopupActionsRowContentConfiguration*)configuration {
  self = [super init];
  if (self) {
    _actionsScrollView = [[UIScrollView alloc] init];
    _actionsScrollView.translatesAutoresizingMaskIntoConstraints = NO;
    _actionsScrollView.showsHorizontalScrollIndicator = NO;

    _actionsStackView = [[UIStackView alloc] init];
    _actionsStackView.translatesAutoresizingMaskIntoConstraints = NO;
    _actionsStackView.alignment = UIStackViewAlignmentCenter;
    _actionsStackView.axis = UILayoutConstraintAxisHorizontal;
    _actionsStackView.spacing = kButtonSpace;

    for (SuggestAction* action in configuration.actions) {
      UIButton* actionButton =
          [[self class] actionButtonWithSuggestAction:action];
      [_actionsStackView addArrangedSubview:actionButton];
    }

    // add a spacer to the stack view in order to fill the available stack view
    // width.
    UIView* spacerView = [[UIView alloc] init];
    spacerView.translatesAutoresizingMaskIntoConstraints = NO;
    [spacerView
        setContentCompressionResistancePriority:0
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    [_actionsStackView addArrangedSubview:spacerView];

    [_actionsScrollView addSubview:_actionsStackView];
    [self addSubview:_actionsScrollView];
    AddSameConstraints(self, _actionsScrollView);
    AddSameConstraintsWithInsets(
        _actionsStackView, _actionsScrollView.contentLayoutGuide,
        NSDirectionalEdgeInsetsMake(0, kLeadingTrailingPadding, 0,
                                    kLeadingTrailingPadding));

    [NSLayoutConstraint activateConstraints:@[
      [self.heightAnchor
          constraintGreaterThanOrEqualToConstant:kOmniboxPopupActionsHeight],
      [_actionsScrollView.heightAnchor
          constraintEqualToAnchor:_actionsStackView.heightAnchor],
      // Constraint the stackview's width to be able to scroll to semantic
      // leading.
      [_actionsStackView.widthAnchor
          constraintGreaterThanOrEqualToAnchor:_actionsScrollView
                                                   .frameLayoutGuide.widthAnchor
                                      constant:-2 * kLeadingTrailingPadding]
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

#pragma mark - Private

// Creates a suggestion action button and setup its configutation attributes
// (eg. Call action button).
+ (UIButton*)actionButtonWithSuggestAction:(SuggestAction*)suggestAction {
  UIButton* button = [[UIButton alloc] init];
  button.layer.cornerRadius = kButtonLayerRadius;
  button.layer.masksToBounds = YES;
  [button
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [button
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisVertical];

  NSAttributedString* attributedTitle = [[NSAttributedString alloc]
      initWithString:suggestAction.title
          attributes:@{
            NSFontAttributeName :
                [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote]
          }];

  UIButtonConfiguration* configuration =
      [UIButtonConfiguration plainButtonConfiguration];
  configuration.imagePadding = kButtonTitleImagePadding;
  configuration.attributedTitle = attributedTitle;
  configuration.image = [SuggestAction imageIconForAction:suggestAction
                                                     size:kIconSize];
  UIBackgroundConfiguration* backgroundConfig =
      [UIBackgroundConfiguration clearConfiguration];
  backgroundConfig.backgroundColor =
      [UIColor colorNamed:kFaviconBackgroundColor];
  configuration.background = backgroundConfig;

  button.configuration = configuration;

  return button;
}

@end
