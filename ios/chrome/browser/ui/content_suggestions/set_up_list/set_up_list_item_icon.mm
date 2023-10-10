// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_item_icon.h"

#import "base/feature_list.h"
#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "components/sync/base/features.h"
#import "ios/chrome/browser/ntp/home/features.h"
#import "ios/chrome/browser/ntp/set_up_list_item_type.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Constants related to icon sizing.
constexpr CGFloat kIconSize = 36;
constexpr CGFloat kMagicStackIconSize = 30;
constexpr CGFloat kCompactIconSize = 26;
constexpr CGFloat kSymbolPointSize = 18;
constexpr CGFloat kSparkleSize = 72;
constexpr CGFloat kSparkleOffset = (kSparkleSize - kIconSize) / 2;
constexpr CGFloat kMagicStackSparkleOffset =
    (kSparkleSize - kMagicStackIconSize) / 2;
constexpr CGFloat kIconSquareContainerRadius = 7.0f;

// The amount of rotation for the icons, during the animation.
constexpr CGFloat kAnimationRotation = 90 * M_PI / 180;

// Constants related to the sparkle animation frame images.
constexpr NSString* const kAnimationSparkleFrames = @"set_up_list_sparkle%d";
constexpr int kAnimationSparkleFrameCount = 36;

// Returns a UIImageView for the given SF Symbol, and with a color named
// `colorName`.
UIImageView* IconForSymbol(NSString* symbol,
                           BOOL compact_layout,
                           NSArray<UIColor*>* color_palette = nil) {
  UIImageSymbolConfiguration* config = [UIImageSymbolConfiguration
      configurationWithWeight:UIImageSymbolWeightLight];
  if (color_palette) {
    UIImageSymbolConfiguration* colorConfig = [UIImageSymbolConfiguration
        configurationWithPaletteColors:color_palette];
    config = [config configurationByApplyingConfiguration:colorConfig];
  }
  UIImage* image = DefaultSymbolWithConfiguration(symbol, config);
  UIImageView* icon = [[UIImageView alloc] initWithImage:image];
  icon.translatesAutoresizingMaskIntoConstraints = NO;
  CGFloat nonCompactIconWidth =
      IsMagicStackEnabled() ? kMagicStackIconSize : kIconSize;
  CGFloat iconWidth = compact_layout ? kCompactIconSize : nonCompactIconWidth;
  [NSLayoutConstraint activateConstraints:@[
    [icon.widthAnchor constraintEqualToConstant:iconWidth],
    [icon.heightAnchor constraintEqualToAnchor:icon.widthAnchor],
  ]];
  return icon;
}

UIView* IconInSquareContainer(UIImageView* icon, NSString* color) {
  UIView* square_view = [[UIView alloc] init];
  square_view.translatesAutoresizingMaskIntoConstraints = NO;
  square_view.layer.cornerRadius = kIconSquareContainerRadius;
  square_view.backgroundColor = [UIColor colorNamed:color];

  [square_view addSubview:icon];
  AddSameCenterConstraints(icon, square_view);
  CGFloat iconWidth = IsMagicStackEnabled() ? kMagicStackIconSize : kIconSize;
  [NSLayoutConstraint activateConstraints:@[
    [square_view.widthAnchor constraintEqualToConstant:iconWidth],
    [square_view.heightAnchor constraintEqualToAnchor:square_view.widthAnchor],
  ]];
  return square_view;
}

UIImageView* DefaultBrowserIcon(BOOL compact_layout) {
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  UIImageView* container = [[UIImageView alloc] init];
  container.translatesAutoresizingMaskIntoConstraints = NO;
  // The custom symbol Chrome icon has the circle around the blue center as
  // transparent. We can make it appear white by adding a white circle
  // behind it.
  UIImageView* circle = [[UIImageView alloc]
      initWithImage:DefaultSymbolWithPointSize(kCircleFillSymbol,
                                               kSymbolPointSize)];
  circle.tintColor = [UIColor whiteColor];
  circle.translatesAutoresizingMaskIntoConstraints = NO;
  [container addSubview:circle];
  AddSameCenterConstraints(circle, container);
  // Create the main icon view.
  UIImage* image = MakeSymbolMulticolor(
      CustomSymbolWithPointSize(kChromeSymbol, kSymbolPointSize));
  UIImageView* icon = [[UIImageView alloc] initWithImage:image];
  icon.translatesAutoresizingMaskIntoConstraints = NO;
  [container addSubview:icon];
  AddSameConstraints(icon, container);
  // Set the widths.
  CGFloat nonCompactIconWidth =
      IsMagicStackEnabled() ? kMagicStackIconSize : kIconSize;
  CGFloat icon_width = compact_layout ? kCompactIconSize : nonCompactIconWidth;
  // The white background circle must be smaller so that the edges don't show.
  CGFloat white_background_width = icon_width - 2;
  [NSLayoutConstraint activateConstraints:@[
    [container.widthAnchor constraintEqualToConstant:icon_width],
    [container.heightAnchor constraintEqualToAnchor:container.widthAnchor],
    [circle.widthAnchor constraintEqualToConstant:white_background_width],
    [circle.heightAnchor constraintEqualToAnchor:circle.widthAnchor],
  ]];
  return container;
#else
  return IconForSymbol(kDefaultBrowserSymbol, compact_layout);
#endif
}

// Returns a UIImageView containing the given symbol enclosed in a filled
// circle. The circle's color will be the color named `circleColorName`.
// Note: this was necessary because there was no symbol exactly matching
// what was needed for the Autofill icon.
UIImageView* IconInCircle(NSString* symbol,
                          BOOL compact_layout,
                          NSString* circle_color_name) {
  UIImageView* circle_view = IconForSymbol(kCircleFillSymbol, compact_layout);
  circle_view.tintColor = [UIColor colorNamed:circle_color_name];
  UIImageConfiguration* compactImageConfiguration = [UIImageSymbolConfiguration
      configurationWithPointSize:kSymbolPointSize
                          weight:UIImageSymbolWeightLight
                           scale:UIImageSymbolScaleSmall];
  UIImage* symbol_image =
      compact_layout
          ? DefaultSymbolWithConfiguration(symbol, compactImageConfiguration)
          : DefaultSymbolWithPointSize(symbol, kSymbolPointSize);
  CHECK(symbol_image);

  UIImageView* symbol_view = [[UIImageView alloc] initWithImage:symbol_image];
  symbol_view.tintColor = [UIColor whiteColor];
  symbol_view.translatesAutoresizingMaskIntoConstraints = NO;
  [circle_view addSubview:symbol_view];
  AddSameCenterConstraints(symbol_view, circle_view);
  return circle_view;
}

UIView* IconInSquare(NSString* symbol,
                     BOOL compact_layout,
                     NSString* color_name) {
  UIImageConfiguration* compactImageConfiguration = [UIImageSymbolConfiguration
      configurationWithPointSize:kSymbolPointSize
                          weight:UIImageSymbolWeightLight
                           scale:UIImageSymbolScaleSmall];
  UIView* square_view = [[UIView alloc] init];
  square_view.translatesAutoresizingMaskIntoConstraints = NO;
  square_view.layer.cornerRadius = kIconSquareContainerRadius;
  square_view.backgroundColor = [UIColor colorNamed:color_name];
  UIImage* symbol_image =
      compact_layout
          ? DefaultSymbolWithConfiguration(symbol, compactImageConfiguration)
          : DefaultSymbolWithPointSize(symbol, kSymbolPointSize);
  CHECK(symbol_image);

  UIImageView* symbol_view = [[UIImageView alloc] initWithImage:symbol_image];
  symbol_view.tintColor = [UIColor whiteColor];
  symbol_view.translatesAutoresizingMaskIntoConstraints = NO;
  [square_view addSubview:symbol_view];
  AddSameCenterConstraints(symbol_view, square_view);
  CGFloat iconWidth = IsMagicStackEnabled() ? kMagicStackIconSize : kIconSize;
  [NSLayoutConstraint activateConstraints:@[
    [square_view.widthAnchor constraintEqualToConstant:iconWidth],
    [square_view.heightAnchor constraintEqualToAnchor:square_view.widthAnchor],
  ]];
  return square_view;
}

}  // namespace

@implementation SetUpListItemIcon {
  SetUpListItemType _type;
  BOOL _complete;
  UIView* _typeIcon;
  UIImageView* _checkmark;
  UIImageView* _sparkle;
  // YES if this view should configure itself with a compacted layout.
  BOOL _compactLayout;
  // YES if this view should place the icon in a square shape.
  BOOL _inSquare;
}

- (instancetype)initWithType:(SetUpListItemType)type
                    complete:(BOOL)complete
               compactLayout:(BOOL)compactLayout
                    inSquare:(BOOL)inSquare {
  self = [super init];
  if (self) {
    _type = type;
    _complete = complete;
    _compactLayout = compactLayout;
    _inSquare = inSquare;
  }
  return self;
}

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  [super willMoveToSuperview:newSuperview];

  [self createSubviews];
}

#pragma mark - Public methods

- (void)markComplete {
  if (_complete) {
    return;
  }
  _complete = YES;
  _checkmark.transform = CGAffineTransformMakeRotation(0);
  _typeIcon.transform = CGAffineTransformMakeRotation(kAnimationRotation);
  _checkmark.alpha = 1;
  _typeIcon.alpha = 0;
}

- (void)playSparkleWithDuration:(base::TimeDelta)duration
                          delay:(base::TimeDelta)delay {
  // Load the sparkle animation frame images.
  NSMutableArray<UIImage*>* images = [[NSMutableArray alloc] init];
  for (int i = 0; i < kAnimationSparkleFrameCount; i++) {
    NSString* name = [NSString stringWithFormat:kAnimationSparkleFrames, i];
    [images addObject:[UIImage imageNamed:name]];
  }
  _sparkle.animationImages = images;
  _sparkle.animationDuration = duration.InSecondsF();
  _sparkle.animationRepeatCount = 1;

  // Start animating after the given `delay`.
  __weak __typeof(_sparkle) weakSparkle = _sparkle;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(^{
        [weakSparkle startAnimating];
      }),
      delay);
}

#pragma mark - Private methods

// Creates all the subviews: the type-specific icon, the checkmark, and the
// sparkle image view.
- (void)createSubviews {
  // Return if the subviews have already been created and added.
  if (!(self.subviews.count == 0)) {
    return;
  }

  self.tintAdjustmentMode = UIViewTintAdjustmentModeNormal;
  _typeIcon = [self createTypeIcon];
  _checkmark = IconForSymbol(
      kCheckmarkCircleFillSymbol, _compactLayout,
      @[ [UIColor whiteColor], [UIColor colorNamed:kBlue500Color] ]);
  _sparkle = [self createSparkle];
  [self addSubview:_typeIcon];
  [self addSubview:_checkmark];
  [self addSubview:_sparkle];
  AddSameConstraints(_checkmark, _typeIcon);
  AddSameConstraints(self, _typeIcon);
  if (_complete) {
    _typeIcon.alpha = 0;
  } else {
    _checkmark.alpha = 0;
    _checkmark.transform = CGAffineTransformMakeRotation(-kAnimationRotation);
  }
}

// Creates the type-specific icon for this item.
- (UIView*)createTypeIcon {
  switch (_type) {
    case SetUpListItemType::kSignInSync: {
      if (base::FeatureList::IsEnabled(
              syncer::kReplaceSyncPromosWithSignInPromos)) {
        return _inSquare ? IconInSquare(kPersonCropCircleSymbol, _compactLayout,
                                        kGreen500Color)
                         : IconInCircle(kPersonCropCircleSymbol, _compactLayout,
                                        kGreen500Color);
      }

      UIImageView* iconImage = IconForSymbol(
          kSyncCircleSymbol, _compactLayout,
          @[ [UIColor whiteColor], [UIColor colorNamed:kGreen500Color] ]);
      return _inSquare ? IconInSquareContainer(iconImage, kGreen500Color)
                       : iconImage;
    }
    case SetUpListItemType::kDefaultBrowser: {
      UIImageView* iconImage = DefaultBrowserIcon(_compactLayout || _inSquare);
      if (_inSquare) {
        return IconInSquareContainer(iconImage, kBackgroundColor);
      }
      return iconImage;
    }
    case SetUpListItemType::kAutofill: {
      return _inSquare
                 ? IconInSquare(kEllipsisRectangleSymbol, NO, kBlue500Color)
                 : IconInCircle(kEllipsisRectangleSymbol, _compactLayout,
                                kBlue500Color);
    }
    case SetUpListItemType::kAllSet: {
      return IconForSymbol(
          kCheckmarkSealFillSymbol, _compactLayout,
          @[ [UIColor whiteColor], [UIColor colorNamed:kBlue500Color] ]);
    }
    case SetUpListItemType::kFollow:
      // TODO(crbug.com/1428070): Add a Follow item to the Set Up List.
      NOTREACHED();
      return nil;
  }
}

// Creates the image view that plays the "sparkle" animation.
- (UIImageView*)createSparkle {
  // This image view does not initially have an image. The animation frames
  // are loaded on demand.
  UIImageView* imageView = [[UIImageView alloc] initWithImage:nil];
  CGFloat sparkleOffset =
      IsMagicStackEnabled() ? kMagicStackSparkleOffset : kSparkleOffset;
  imageView.frame =
      CGRectMake(-sparkleOffset, -sparkleOffset, kSparkleSize, kSparkleSize);
  return imageView;
}

@end
