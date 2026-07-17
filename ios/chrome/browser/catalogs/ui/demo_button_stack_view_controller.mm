// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/catalogs/ui/demo_button_stack_view_controller.h"

#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation DemoButtonStackViewController {
  // Determines whether the contentView needs to be large enough to be
  // scrollable.
  BOOL _isContentScrollable;
}

- (instancetype)initWithConfiguration:(ButtonStackConfiguration*)configuration
                    scrollableContent:(BOOL)isContentScrollable {
  self = [super initWithConfiguration:configuration];
  if (self) {
    _isContentScrollable = isContentScrollable;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  UILabel* label = [[UILabel alloc] init];
  label.text = @"This is a scrollable content view";
  label.font = [UIFont systemFontOfSize:24];
  label.textAlignment = NSTextAlignmentCenter;
  label.translatesAutoresizingMaskIntoConstraints = NO;
  [label.heightAnchor constraintGreaterThanOrEqualToConstant:50.0].active = YES;

  UIImageView* imageView = [[UIImageView alloc]
      initWithImage:[UIImage imageNamed:@"collaboration_signin_background"]];
  imageView.translatesAutoresizingMaskIntoConstraints = NO;

  NSMutableArray<UIView*>* subViews = [@[ label, imageView ] mutableCopy];

  if (_isContentScrollable) {
    UIView* blueSquare = [[UIView alloc] init];
    blueSquare.backgroundColor = [UIColor blueColor];
    blueSquare.translatesAutoresizingMaskIntoConstraints = NO;
    [blueSquare.heightAnchor constraintGreaterThanOrEqualToConstant:1000.0]
        .active = YES;

    [subViews addObject:blueSquare];
  }

  UIStackView* stackView =
      [[UIStackView alloc] initWithArrangedSubviews:subViews];
  stackView.axis = UILayoutConstraintAxisVertical;
  stackView.translatesAutoresizingMaskIntoConstraints = NO;

  [self.contentView addSubview:stackView];

  AddSameConstraints(self.contentView, stackView);
}

@end
