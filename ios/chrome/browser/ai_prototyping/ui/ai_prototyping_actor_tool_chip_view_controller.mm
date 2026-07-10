// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_actor_tool_chip_view_controller.h"

#import "ios/chrome/browser/intelligence/actor/ui/actor_tool_chip_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Unified spacing values between elements
const CGFloat kSpacing = 12.0;

const CGFloat kDotWidth = 12.0;
const CGFloat kSymbolPointSize = 18.0;

// Data representing an actor tool chip.
struct ChipData {
  NSString* symbol_name;
  NSString* text;
};

}  // namespace

@implementation AIPrototypingActorToolChipViewController {
  UIScrollView* _scrollView;
  NSMutableArray<ActorToolChipView*>* _chips;
  NSArray<NSString*>* _colorNames;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = @"Actor Tool Chips";
  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];

  _colorNames = @[ kTextSecondaryColor, kBlueColor, kGreenColor, kRedColor ];
  _chips = [NSMutableArray array];

  NSMutableArray<UIImage*>* colorImages = [NSMutableArray array];
  for (NSString* colorName in _colorNames) {
    UIGraphicsImageRenderer* renderer = [[UIGraphicsImageRenderer alloc]
        initWithSize:CGSizeMake(kDotWidth, kDotWidth)];
    UIImage* image =
        [renderer imageWithActions:^(UIGraphicsImageRendererContext* context) {
          [[UIColor colorNamed:colorName] setFill];
          [[UIBezierPath bezierPathWithOvalInRect:CGRectMake(0, 0, kDotWidth,
                                                             kDotWidth)] fill];
        }];
    [colorImages addObject:[image imageWithRenderingMode:
                                      UIImageRenderingModeAlwaysOriginal]];
  }

  UISegmentedControl* colorSelector =
      [[UISegmentedControl alloc] initWithItems:colorImages];
  colorSelector.selectedSegmentIndex = 0;
  colorSelector.translatesAutoresizingMaskIntoConstraints = NO;
  [colorSelector addTarget:self
                    action:@selector(onColorSelectorChanged:)
          forControlEvents:UIControlEventValueChanged];

  _scrollView = [[UIScrollView alloc] init];
  _scrollView.translatesAutoresizingMaskIntoConstraints = NO;

  UIStackView* containerStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ colorSelector, _scrollView ]];
  containerStack.axis = UILayoutConstraintAxisVertical;
  containerStack.spacing = kSpacing;
  containerStack.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:containerStack];

  UIStackView* stackView = [[UIStackView alloc] init];
  stackView.axis = UILayoutConstraintAxisVertical;
  stackView.spacing = kSpacing * 2.0;
  stackView.alignment = UIStackViewAlignmentCenter;
  stackView.translatesAutoresizingMaskIntoConstraints = NO;
  [_scrollView addSubview:stackView];

  AddSameConstraintsWithInsets(
      containerStack, self.view.safeAreaLayoutGuide,
      NSDirectionalEdgeInsetsMake(kSpacing, kSpacing, kSpacing, kSpacing));
  AddSameConstraintsWithInsets(
      stackView, _scrollView.contentLayoutGuide,
      NSDirectionalEdgeInsetsMake(kSpacing * 2.0, 0, kSpacing * 2.0, 0));
  [NSLayoutConstraint activateConstraints:@[
    [stackView.widthAnchor
        constraintEqualToAnchor:_scrollView.frameLayoutGuide.widthAnchor],
  ]];

  const ChipData kChips[] = {
      {kCursorArrowRaysSymbol, @"Clicking"},
      {kCursorArrowSymbol, @"Working"},
      {kKeyboardSymbol, @"Typing"},
      {kCursorArrowMotionLinesSymbol, @"Scrolling"},
      {kHourglassSymbol, @"Waiting"},
      {kKeySymbol, @"Filling password"},
  };
  for (const ChipData& data : kChips) {
    ActorToolChipView* chip = [[ActorToolChipView alloc]
        initWithText:data.text
                icon:DefaultSymbolWithPointSize(data.symbol_name,
                                                kSymbolPointSize)];
    [stackView addArrangedSubview:chip];
    [_chips addObject:chip];
  }
}

#pragma mark - Actions

- (void)onColorSelectorChanged:(UISegmentedControl*)sender {
  NSInteger index = sender.selectedSegmentIndex;
  if (index >= 0 && (NSUInteger)index < _colorNames.count) {
    UIColor* selectedColor = [UIColor colorNamed:_colorNames[index]];
    for (ActorToolChipView* chip in _chips) {
      chip.tintColor = selectedColor;
    }
  }
}

@end
