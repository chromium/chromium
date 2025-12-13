// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/switch_content_view.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/switch_content_configuration.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation SwitchContentView {
  // The configuration of the view.
  SwitchContentConfiguration* _configuration;
  // The switch.
  UISwitch* _switchView;
}

- (instancetype)initWithConfiguration:
    (SwitchContentConfiguration*)configuration {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _switchView = [[UISwitch alloc] init];
    _switchView.translatesAutoresizingMaskIntoConstraints = NO;
    [_switchView setContentHuggingPriority:UILayoutPriorityRequired - 1
                                   forAxis:UILayoutConstraintAxisHorizontal];
    [_switchView
        setContentCompressionResistancePriority:UILayoutPriorityRequired
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    [self addSubview:_switchView];

    _configuration = [configuration copy];
    [self applyConfiguration];

    AddSameConstraints(_switchView, self);
  }
  return self;
}

- (UISwitch*)switchForTesting {
  return _switchView;
}

#pragma mark - ChromeContentView

- (BOOL)hasCustomAccessibilityActivationPoint {
  return YES;
}

#pragma mark - UIContentView

- (id<UIContentConfiguration>)configuration {
  return _configuration;
}

- (void)setConfiguration:(id<UIContentConfiguration>)configuration {
  _configuration =
      [base::apple::ObjCCastStrict<SwitchContentConfiguration>(configuration)
          copy];
  [self applyConfiguration];
}

- (BOOL)supportsConfiguration:(id<UIContentConfiguration>)configuration {
  return [configuration isMemberOfClass:SwitchContentConfiguration.class];
}

#pragma mark - UIAccessibility

- (CGPoint)accessibilityActivationPoint {
  CGRect frameInScreenCoordinates =
      UIAccessibilityConvertFrameToScreenCoordinates(_switchView.bounds,
                                                     _switchView);
  return CGPointMake(CGRectGetMidX(frameInScreenCoordinates),
                     CGRectGetMidY(frameInScreenCoordinates));
}

#pragma mark - Private

// Updates the content view with the current configuration.
- (void)applyConfiguration {
  [_switchView removeTarget:nil
                     action:NULL
           forControlEvents:UIControlEventAllEvents];
  [_switchView addTarget:_configuration.target
                  action:_configuration.selector
        forControlEvents:UIControlEventValueChanged];
  _switchView.tag = _configuration.tag;
  _switchView.on = _configuration.on;
  _switchView.enabled = _configuration.enabled;
}

@end
