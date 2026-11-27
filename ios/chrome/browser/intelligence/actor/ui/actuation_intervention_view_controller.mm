// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/actuation_intervention_view_controller.h"

#import "ios/chrome/browser/intelligence/actor/ui/actuation_intervention_mutator.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_intervention_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@interface ActuationInterventionViewController () <
    ActuationInterventionViewDelegate>
@end

@implementation ActuationInterventionViewController {
  // Displays `_intervention`. Owns its padding and collapses to zero height
  // when empty.
  ActuationInterventionView* _interventionView;
  // Intervention displayed by `_interventionView`, if any.
  ActuationInterventionData* _intervention;
}

#pragma mark - Public

- (instancetype)init {
  return [super initWithNibName:nil bundle:nil];
}

- (CGFloat)fittingHeight {
  [self loadViewIfNeeded];
  CGFloat width = self.view.bounds.size.width;
  if (width <= 0) {
    return [_interventionView
               systemLayoutSizeFittingSize:UILayoutFittingCompressedSize]
        .height;
  }
  CGSize targetSize = CGSizeMake(width, UILayoutFittingCompressedSize.height);
  return [_interventionView
               systemLayoutSizeFittingSize:targetSize
             withHorizontalFittingPriority:UILayoutPriorityRequired
                   verticalFittingPriority:UILayoutPriorityFittingSizeLevel]
      .height;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  _interventionView = [[ActuationInterventionView alloc] init];
  _interventionView.delegate = self;
  _interventionView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_interventionView];
  AddSameConstraints(_interventionView, self.view);
  [_interventionView configureWithData:_intervention];
}

#pragma mark - ActuationInterventionConsumer

- (void)setIntervention:(ActuationInterventionData*)intervention {
  // Configuring `_interventionView` again would redraw its buttons.
  if (_intervention == intervention) {
    return;
  }
  _intervention = intervention;
  [_interventionView configureWithData:_intervention];
  [self.delegate interventionViewControllerDidChangeHeight:self];
}

#pragma mark - ActuationInterventionViewDelegate

- (void)interventionView:(ActuationInterventionView*)view
        didTriggerAction:(ActuationInterventionAction*)action {
  [self.mutator didTriggerInterventionAction:action];
}

@end
