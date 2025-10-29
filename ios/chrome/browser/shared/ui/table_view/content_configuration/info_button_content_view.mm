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

namespace {
const CGFloat kInfoSymbolSize = 22;
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

    AddSameConstraints(_infoButton, self);
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

@end
