// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/info_button_content_view.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/info_button_content_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
const CGFloat kInfoSymbolSize = 22;
const CGFloat kButtonSize = 27;
}

@implementation InfoButtonContentView {
  // The configuration of the view.
  InfoButtonContentConfiguration* _configuration;
  // The info button.
  UIButton* _infoButton;
}

- (instancetype)initWithConfiguration:
    (InfoButtonContentConfiguration*)configuration {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    UIButtonConfiguration* buttonConfig =
        [UIButtonConfiguration plainButtonConfiguration];
    buttonConfig.image =
        DefaultSymbolTemplateWithPointSize(kInfoCircleSymbol, kInfoSymbolSize);
    buttonConfig.contentInsets = NSDirectionalEdgeInsetsZero;
    _infoButton = [UIButton buttonWithType:UIButtonTypeSystem];
    _infoButton.configuration = buttonConfig;
    _infoButton.tintColor = [UIColor colorNamed:kBlueColor];
    _infoButton.accessibilityIdentifier = kTableViewCellInfoButtonViewId;
    _infoButton.translatesAutoresizingMaskIntoConstraints = NO;
    [_infoButton setContentHuggingPriority:UILayoutPriorityRequired - 1
                                   forAxis:UILayoutConstraintAxisHorizontal];
    [_infoButton
        setContentCompressionResistancePriority:UILayoutPriorityRequired
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    [self addSubview:_infoButton];

    _configuration = [configuration copy];
    [self applyConfiguration];

    [NSLayoutConstraint activateConstraints:@[
      [self.widthAnchor constraintEqualToConstant:kButtonSize],
      [self.heightAnchor constraintEqualToAnchor:self.widthAnchor],
      [_infoButton.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
      [_infoButton.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
    ]];
  }
  return self;
}

- (UIButton*)infoButtonForTesting {
  return _infoButton;
}

#pragma mark - ChromeContentView

- (BOOL)hasCustomAccessibilityActivationPoint {
  return _configuration.selectedForVoiceOver;
}

#pragma mark - UIContentView

- (id<UIContentConfiguration>)configuration {
  return _configuration;
}

- (void)setConfiguration:(id<UIContentConfiguration>)configuration {
  _configuration = [base::apple::ObjCCastStrict<InfoButtonContentConfiguration>(
      configuration) copy];
  [self applyConfiguration];
}

- (BOOL)supportsConfiguration:(id<UIContentConfiguration>)configuration {
  return [configuration isMemberOfClass:InfoButtonContentConfiguration.class];
}

#pragma mark - UIAccessibility

- (CGPoint)accessibilityActivationPoint {
  CGRect frameInScreenCoordinates =
      UIAccessibilityConvertFrameToScreenCoordinates(_infoButton.bounds,
                                                     _infoButton);
  return CGPointMake(CGRectGetMidX(frameInScreenCoordinates),
                     CGRectGetMidY(frameInScreenCoordinates));
}

- (NSArray<UIAccessibilityCustomAction*>*)accessibilityCustomActions {
  if (!_configuration.selectedForVoiceOver) {
    UIAccessibilityCustomAction* tapButtonAction =
        [[UIAccessibilityCustomAction alloc]
            initWithName:l10n_util::GetNSString(
                             IDS_IOS_INFO_BUTTON_ACCESSIBILITY_HINT)
                  target:self
                selector:@selector(handleButtonVoiceOverActivation)];
    return @[ tapButtonAction ];
  }
  return [super accessibilityCustomActions];
}

#pragma mark - Private

// Updates the content view with the current configuration.
- (void)applyConfiguration {
  [_infoButton removeTarget:nil
                     action:NULL
           forControlEvents:UIControlEventAllEvents];
  [_infoButton addTarget:_configuration.target
                  action:_configuration.selector
        forControlEvents:UIControlEventTouchUpInside];
  _infoButton.tag = _configuration.tag;
  _infoButton.enabled = _configuration.enabled;
}

// Simulates a tap on the button.
- (void)handleButtonVoiceOverActivation {
  [_infoButton sendActionsForControlEvents:UIControlEventTouchUpInside];
}

@end
