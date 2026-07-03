// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/ui/level_up_progress_bar.h"

#import "ios/chrome/browser/shared/ui/elements/gradient/multi_color_gradient_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// The spacing inside the progress horizontal stack view.
constexpr CGFloat kProgressStackSpacing = 2.0;
// The thickness height of a progress task.
constexpr CGFloat kTaskHeight = 10.0;

}  // namespace

@implementation LevelUpProgressBar {
  // View containing the progress tasks.
  UIStackView* _tasksView;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.translatesAutoresizingMaskIntoConstraints = NO;

    _tasksView = [[UIStackView alloc] init];
    _tasksView.axis = UILayoutConstraintAxisHorizontal;
    _tasksView.spacing = kProgressStackSpacing;
    _tasksView.distribution = UIStackViewDistributionFillEqually;
    _tasksView.translatesAutoresizingMaskIntoConstraints = NO;

    [self addSubview:_tasksView];
    AddSameConstraints(_tasksView, self);
  }
  return self;
}

- (void)setCompleted:(NSInteger)completed total:(NSInteger)total {
  // Clear previous segments.
  [_tasksView.arrangedSubviews
      makeObjectsPerformSelector:@selector(removeFromSuperview)];

  for (NSInteger i = 0; i < total; i++) {
    UIView* taskView = nil;
    if (i < completed) {
      NSArray<UIColor*>* colors = @[
        [UIColor colorNamed:kBlueColor], [UIColor colorNamed:kBlue300Color]
      ];
      // Each task shows a slice of one continuous gradient spanning
      // global positions [globalStartPoint → globalEndPoint]
      // ([0 → completed]).
      //
      // Task `i` occupies global range [i → i + 1],
      // but draws in its own local space [0 → 1].
      //
      // To map global gradient positions into this task’s local space:
      // localPoint = globalPoint - i
      NSInteger globalStartPoint = 0;
      NSInteger globalEndPoint = completed;
      CGPoint localStartPoint = CGPointMake(globalStartPoint - i, 0.5);
      CGPoint localEndPoint = CGPointMake(globalEndPoint - i, 0.5);

      taskView = [[MultiColorGradientView alloc] initWithColors:colors
                                                      locations:nil
                                                     startPoint:localStartPoint
                                                       endPoint:localEndPoint];
    } else {
      taskView = [[UIView alloc] init];
      taskView.backgroundColor = [UIColor colorNamed:kBlueHaloColor];
    }

    taskView.translatesAutoresizingMaskIntoConstraints = NO;
    taskView.layer.cornerRadius = kTaskHeight / 2;
    taskView.layer.masksToBounds = YES;
    [taskView.heightAnchor constraintEqualToConstant:kTaskHeight].active = YES;

    [_tasksView addArrangedSubview:taskView];
  }
}

@end
