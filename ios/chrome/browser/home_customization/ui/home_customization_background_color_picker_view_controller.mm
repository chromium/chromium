// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_background_color_picker_view_controller.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "ios/chrome/browser/home_customization/ui/background_collection_configuration.h"
#import "ios/chrome/browser/home_customization/ui/background_customization_configuration.h"
#import "ios/chrome/browser/home_customization/ui/centered_flow_layout.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_accessibility_identifiers.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_configuration_mutator.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_background_picker_presentation_delegate.h"
#import "ios/chrome/browser/home_customization/ui/home_customization_custom_color_cell.h"
#import "ios/chrome/browser/home_customization/ui/home_cutomization_color_palette_cell.h"
#import "ios/chrome/browser/home_customization/ui/rainbow_slider.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

// Define constants within the namespace
namespace {
// The width and height of each color palette cell in the collection view.
const CGFloat kColorCellSize = 60.0;

// The vertical spacing between rows of cells.
const CGFloat kLineSpacing = 23.0;

// The horizontal spacing between cells in the same row.
const CGFloat kItemSpacing = 2.0;

// The top padding for the section in the collection view.
const CGFloat kSectionInsetTop = 0.0;

// The left and right padding for the section in the collection view.
const CGFloat kSectionInsetSides = 25.0;

// The left and right padding for the footer view.
const CGFloat kFooterInsetSides = 32.0;

// The bottom padding for the section in the collection view.
const CGFloat kSectionInsetBottom = 27.0;

// The portion of the width taken by the color cells (from 0.0 to 1.0).
const CGFloat kRelativeRowWidth = 1.0;

// Animation duration (in seconds) for the footer sliding up when the custom
// color is selected.
const CGFloat kFooterSlideInDuration = 0.3;

// Damping ratio for the spring animation (1.0 = no bounce, lower = more
// bounce).
const CGFloat kFooterSlideInDamping = 0.8;

// Initial velocity for the spring animation, controlling the starting speed of
// the movement.
const CGFloat kFooterSlideInVelocity = 0.4;

// Returns a dynamic UIColor using two named color assets for light and dark
// mode.
UIColor* DynamicNamedColor(NSString* lightName, NSString* darkName) {
  return
      [UIColor colorWithDynamicProvider:^UIColor*(UITraitCollection* traits) {
        BOOL isDark = (traits.userInterfaceStyle == UIUserInterfaceStyleDark);
        return [UIColor colorNamed:isDark ? darkName : lightName];
      }];
}

}  // namespace

@interface HomeCustomizationBackgroundColorPickerViewController () {
  // This view controller's main collection view for displaying content.
  UICollectionView* _collectionView;

  // The background collection configuration to be displayed in this view
  // controller.
  BackgroundCollectionConfiguration* _backgroundCollectionConfiguration;

  // The `UICollectionViewCellRegistration` for registering  and configuring the
  // `HomeCustomizationColorPaletteCell` in the collection view.
  UICollectionViewCellRegistration* _colorCellRegistration;

  // The `UICollectionViewCellRegistration` for registering  and configuring the
  // `HomeCustomizationCustomColorCell` in the collection view.
  UICollectionViewCellRegistration* _customColorCellRegistration;

  // The `UICollectionViewSupplementaryRegistration`for registering the footer
  // of the collection view.
  UICollectionViewSupplementaryRegistration* _footerRegistration;

  // Selected color id on initial load.
  NSString* _initialSelectedColorID;

  // The custom color configuration.
  id<BackgroundCustomizationConfiguration> _customColorConfiguration;

  // The number of times a color option is selected.
  int _colorClickCount;

  //  Slider used in the footer to adjust the custom background color.
  RainbowSlider* _customColorSlider;
}

@end

@implementation HomeCustomizationBackgroundColorPickerViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  __weak __typeof(self) weakSelf = self;
  self.title = l10n_util::GetNSStringWithFixup(
      IDS_IOS_HOME_CUSTOMIZATION_BACKGROUND_PICKER_COLOR_TITLE);

  if (@available(iOS 26, *)) {
    self.view.backgroundColor = UIColor.clearColor;
  } else {
    self.view.backgroundColor =
        [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  }

  UICollectionViewFlowLayout* layout = [[CenteredFlowLayout alloc] init];

  layout.itemSize = CGSizeMake(kColorCellSize, kColorCellSize);
  layout.minimumLineSpacing = kLineSpacing;
  layout.minimumInteritemSpacing = kItemSpacing;
  layout.sectionInset =
      UIEdgeInsetsMake(kSectionInsetTop, kSectionInsetSides,
                       kSectionInsetBottom, kSectionInsetSides);

  _colorCellRegistration = [UICollectionViewCellRegistration
      registrationWithCellClass:[HomeCustomizationColorPaletteCell class]
           configurationHandler:^(HomeCustomizationColorPaletteCell* cell,
                                  NSIndexPath* indexPath,
                                  id<BackgroundCustomizationConfiguration>
                                      backgroundConfiguration) {
             [weakSelf configureBackgroundCell:cell
                       backgroundConfiguration:backgroundConfiguration
                                   atIndexPath:indexPath];
           }];

  if (IsNTPBackgroundColorSliderEnabled()) {
    layout.footerReferenceSize = CGSizeMake(self.view.frame.size.width, 50.0);

    _customColorSlider = [[RainbowSlider alloc] init];
    _customColorSlider.translatesAutoresizingMaskIntoConstraints = NO;
    _customColorSlider.color = _customColorConfiguration.backgroundColor;
    [_customColorSlider addTarget:self
                           action:@selector(customColorChanged:)
                 forControlEvents:UIControlEventValueChanged];

    _customColorCellRegistration = [UICollectionViewCellRegistration
        registrationWithCellClass:[HomeCustomizationCustomColorCell class]
             configurationHandler:^(HomeCustomizationCustomColorCell* cell,
                                    NSIndexPath* indexPath,
                                    id<BackgroundCustomizationConfiguration>
                                        backgroundConfiguration) {
               [weakSelf configureCustomColorCell:cell];
             }];

    _footerRegistration = [UICollectionViewSupplementaryRegistration
        registrationWithSupplementaryClass:[UICollectionReusableView class]
                               elementKind:UICollectionElementKindSectionFooter
                      configurationHandler:^(UICollectionReusableView* footer,
                                             NSString* elementKind,
                                             NSIndexPath* indexPath) {
                        [weakSelf configureFooterView:footer];
                      }];
  }

  _collectionView = [[UICollectionView alloc] initWithFrame:CGRectZero
                                       collectionViewLayout:layout];
  _collectionView.dataSource = self;
  _collectionView.delegate = self;
  _collectionView.translatesAutoresizingMaskIntoConstraints = NO;
  _collectionView.backgroundColor = UIColor.clearColor;
  [self.view addSubview:_collectionView];

  [self selectInitialColor];

  [NSLayoutConstraint activateConstraints:@[
    [_collectionView.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [_collectionView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [_collectionView.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    [_collectionView.widthAnchor constraintEqualToAnchor:self.view.widthAnchor
                                              multiplier:kRelativeRowWidth],
  ]];
}

- (void)viewWillDisappear:(BOOL)animated {
  base::UmaHistogramCounts10000(
      "IOS.HomeCustomization.Background.Color.ClickCount", _colorClickCount);
  [super viewWillDisappear:animated];
}

#pragma mark - HomeCustomizationBackgroundConfigurationConsumer

- (void)setBackgroundCollectionConfigurations:
            (NSArray<BackgroundCollectionConfiguration*>*)
                backgroundCollectionConfigurations
                         selectedBackgroundId:(NSString*)selectedBackgroundId {
  CHECK(backgroundCollectionConfigurations.count == 1);
  _backgroundCollectionConfiguration =
      backgroundCollectionConfigurations.firstObject;

  // Assuming the last configuration represents the custom color option.
  _customColorConfiguration =
      _backgroundCollectionConfiguration.configurations
          [_backgroundCollectionConfiguration.configurationOrder.lastObject];

  _initialSelectedColorID = selectedBackgroundId;

  [self selectInitialColor];
}

- (void)currentBackgroundConfigurationChanged:
    (id<BackgroundCustomizationConfiguration>)currentConfiguration {
  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(cancelButtonPressed)];
  cancelButton.accessibilityIdentifier =
      kPickerViewCancelButtonAccessibilityIdentifier;

  UIBarButtonItem* doneButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(donebuttonPressed)];
  doneButton.accessibilityIdentifier =
      kPickerViewDoneButtonAccessibilityIdentifier;

  self.navigationItem.leftBarButtonItem = cancelButton;
  self.navigationItem.rightBarButtonItem = doneButton;
}

#pragma mark - UICollectionViewDelegate

- (NSInteger)collectionView:(UICollectionView*)collectionView
     numberOfItemsInSection:(NSInteger)section {
  return _backgroundCollectionConfiguration.configurationOrder.count;
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  // The already-selected item can't be selected again.
  return _collectionView.indexPathsForSelectedItems.firstObject != indexPath;
}

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  NSString* selectedID =
      _backgroundCollectionConfiguration.configurationOrder[indexPath.item];
  id<BackgroundCustomizationConfiguration> backgroundConfiguration =
      _backgroundCollectionConfiguration.configurations[selectedID];

  if (IsNTPBackgroundColorSliderEnabled()) {
    BOOL isCustomColorCell = [backgroundConfiguration.configurationID
        isEqualToString:_customColorConfiguration.configurationID];

    [self setFooterVisible:isCustomColorCell];

    if (isCustomColorCell && !backgroundConfiguration.isCustomColor) {
      // If the custom color cell hasn't even been set, don't actually apply the
      // background color until later, when the user drags the slider.
      return;
    }
  }

  [self.mutator applyBackgroundForConfiguration:backgroundConfiguration];

  if (backgroundConfiguration.backgroundStyle ==
      HomeCustomizationBackgroundStyle::kDefault) {
    base::RecordAction(base::UserMetricsAction(
        "IOS.HomeCustomization.Background.ResetDefault.Tapped"));
  }
  _colorClickCount += 1;
}

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  if (indexPath.item >= 0) {
    NSString* itemID =
        _backgroundCollectionConfiguration.configurationOrder[indexPath.item];
    id<BackgroundCustomizationConfiguration> backgroundConfiguration =
        _backgroundCollectionConfiguration.configurations[itemID];

    if (IsNTPBackgroundColorSliderEnabled() &&
        [backgroundConfiguration.configurationID
            isEqualToString:_customColorConfiguration.configurationID]) {
      return [collectionView
          dequeueConfiguredReusableCellWithRegistration:
              _customColorCellRegistration
                                           forIndexPath:indexPath
                                                   item:
                                                       backgroundConfiguration];
    }

    return [collectionView
        dequeueConfiguredReusableCellWithRegistration:_colorCellRegistration
                                         forIndexPath:indexPath
                                                 item:backgroundConfiguration];
  }

  return nil;
}

#pragma mark - UICollectionViewDataSource

- (UICollectionReusableView*)collectionView:(UICollectionView*)collectionView
          viewForSupplementaryElementOfKind:(NSString*)kind
                                atIndexPath:(NSIndexPath*)indexPath {
  if (IsNTPBackgroundColorSliderEnabled() &&
      kind == UICollectionElementKindSectionFooter) {
    return [collectionView
        dequeueConfiguredReusableSupplementaryViewWithRegistration:
            _footerRegistration
                                                      forIndexPath:indexPath];
  }

  return nil;
}

#pragma mark - Private

// Shows or hides the footer with an animation.
- (void)setFooterVisible:(BOOL)visible {
  if (IsNTPBackgroundColorSliderEnabled()) {
    UICollectionReusableView* footerView = [self footerView];

    if (visible) {
      // First scroll to the custom color slider, show the color slider and
      // animate the footer if the custom color cell is selected.
      NSIndexPath* indexPath = [NSIndexPath indexPathForItem:0 inSection:0];

      // We use UICollectionViewLayoutAttributes instead of the footer view
      // itself because the footer may not yet exist when it's offscreen. Layout
      // attributes are always available, allowing us to determine the footer's
      // position even before the footer is created.
      UICollectionViewLayoutAttributes* footerAttributes =
          [_collectionView.collectionViewLayout
              layoutAttributesForSupplementaryViewOfKind:
                  UICollectionElementKindSectionFooter
                                             atIndexPath:indexPath];

      [_collectionView scrollRectToVisible:footerAttributes.frame animated:YES];
      footerView.hidden = NO;
      footerView.transform =
          CGAffineTransformMakeTranslation(0, footerView.bounds.size.height);
      [UIView animateWithDuration:kFooterSlideInDuration
                            delay:0
           usingSpringWithDamping:kFooterSlideInDamping
            initialSpringVelocity:kFooterSlideInVelocity
                          options:UIViewAnimationOptionCurveEaseInOut
                       animations:^{
                         footerView.transform = CGAffineTransformIdentity;
                       }
                       completion:nil];
    } else if (!footerView.hidden) {
      // The footer is only visible if the custom color cell is selected.
      [UIView animateWithDuration:kFooterSlideInDuration
          delay:0
          usingSpringWithDamping:kFooterSlideInDamping
          initialSpringVelocity:kFooterSlideInVelocity
          options:UIViewAnimationOptionCurveEaseInOut
          animations:^{
            footerView.transform = CGAffineTransformMakeTranslation(
                0, footerView.bounds.size.height +
                       20.0);  // 20px is added to hide the footer completely.
          }
          completion:^(BOOL finished) {
            footerView.hidden = YES;
            footerView.transform = CGAffineTransformIdentity;
          }];
    }
  }
}

// Retrieve the light color from the color palette, or use the default color if
// none is available.
- (UIColor*)resolvedColorPaletteLightColor:
    (id<BackgroundCustomizationConfiguration>)backgroundConfiguration {
  return backgroundConfiguration.colorPalette.lightColor
             ?: DynamicNamedColor(@"ntp_background_color", kGrey100Color);
}

// Retrieve the custom color cell corresponding to the last index path in the
// collection view.
- (HomeCustomizationCustomColorCell*)customColorCell {
  NSIndexPath* lastIndexPath = [NSIndexPath
      indexPathForItem:[_collectionView numberOfItemsInSection:0] - 1
             inSection:0];

  return [_collectionView cellForItemAtIndexPath:lastIndexPath];
}

// Retrieve the footer view under the collection view.
- (UICollectionReusableView*)footerView {
  return [_collectionView
      supplementaryViewForElementKind:UICollectionElementKindSectionFooter
                          atIndexPath:[NSIndexPath indexPathForItem:0
                                                          inSection:0]];
}

// Configures the initial state of the custom color cell.
- (void)configureCustomColorCell:(HomeCustomizationCustomColorCell*)cell {
  if (!_customColorConfiguration.isCustomColor) {
    cell.color = DynamicNamedColor(@"ntp_background_color", kGrey100Color);
    return;
  }

  NewTabPageColorPalette* colorPalette =
      [_customColorConfiguration colorPalette];
  cell.color = colorPalette.lightColor;
  cell.accessibilityLabel = _customColorConfiguration.accessibilityName;

  // Since there's a custom color applied, the custom color cell should be
  // initially selected.
  [_collectionView
      selectItemAtIndexPath:
          [NSIndexPath indexPathForItem:[self collectionView:_collectionView
                                            numberOfItemsInSection:0] -
                                        1
                              inSection:0]
                   animated:NO
             scrollPosition:UICollectionViewScrollPositionNone];
}

// Configures the `UICollectionReusableView`.
- (void)configureFooterView:(UICollectionReusableView*)footerView {
  NSIndexPath* selectedIndexPath =
      [_collectionView indexPathsForSelectedItems].firstObject;
  BOOL isCustomColorSelected =
      (selectedIndexPath.item ==
       [self collectionView:_collectionView numberOfItemsInSection:0] - 1);
  footerView.hidden = !isCustomColorSelected;

  [footerView addSubview:_customColorSlider];

  [NSLayoutConstraint activateConstraints:@[
    [_customColorSlider.leadingAnchor
        constraintEqualToAnchor:footerView.leadingAnchor
                       constant:kFooterInsetSides],
    [_customColorSlider.trailingAnchor
        constraintEqualToAnchor:footerView.trailingAnchor
                       constant:-kFooterInsetSides],
  ]];
}

// Configures a `HomeCustomizationColorPaletteCell` with the provided background
// color configuration and selects it if it matches the currently selected
// background color.
- (void)configureBackgroundCell:(HomeCustomizationColorPaletteCell*)cell
        backgroundConfiguration:
            (id<BackgroundCustomizationConfiguration>)backgroundConfiguration
                    atIndexPath:(NSIndexPath*)indexPath {
  if (backgroundConfiguration.backgroundStyle ==
      HomeCustomizationBackgroundStyle::kDefault) {
    NewTabPageColorPalette* defaultColorPalette =
        [[NewTabPageColorPalette alloc] init];

    // The first choice should be the "no background" option (default appearance
    // colors).
    defaultColorPalette.lightColor =
        [self resolvedColorPaletteLightColor:backgroundConfiguration];
    defaultColorPalette.mediumColor =
        [UIColor colorNamed:@"fake_omnibox_solid_background_color"];
    defaultColorPalette.darkColor =
        DynamicNamedColor(kBlueColor, kTextPrimaryColor);
    cell.colorPalette = defaultColorPalette;
    cell.accessibilityLabel = l10n_util::GetNSString(
        IDS_IOS_HOME_CUSTOMIZATION_BACKGROUND_COLOR_DEFAULT_ACCESSIBILITY_LABEL);
  } else {
    cell.colorPalette = backgroundConfiguration.colorPalette;
    cell.accessibilityLabel = backgroundConfiguration.accessibilityName;
  }
}

// Cancels any unsaved changes and dismisses the menu.
- (void)cancelButtonPressed {
  [self.mutator discardBackground];
  [self.presentationDelegate cancelBackgroundPicker];
}

// Dismiss the menu. The current background will be saved on menu dismiss.
- (void)donebuttonPressed {
  [self.presentationDelegate dismissBackgroundPicker];
}

// Callback when the custom color changes.
- (void)customColorChanged:(UISlider*)customColorSlider {
  _customColorConfiguration.backgroundColor =
      [UIColor colorWithHue:customColorSlider.value
                 saturation:1.0
                 brightness:1.0
                      alpha:1.0];
  _customColorConfiguration.isCustomColor = YES;

  NewTabPageColorPalette* colorPalette =
      [_customColorConfiguration colorPalette];
  [self.mutator applyBackgroundForConfiguration:_customColorConfiguration];
  [self customColorCell].color = colorPalette.lightColor;
}

// Selects the initial selected color once the collection view has loaded.
- (void)selectInitialColor {
  NSUInteger selectedIndex =
      [_backgroundCollectionConfiguration.configurationOrder
          indexOfObject:_initialSelectedColorID];
  if (selectedIndex == NSNotFound) {
    _initialSelectedColorID = nil;
    return;
  }
  // Only reset `_initialSelectedColorID` if the collection view has loaded.
  // Otherwise, leave it so the selection can be set when the collection view
  // does load.
  if (_collectionView) {
    [_collectionView
        selectItemAtIndexPath:[NSIndexPath indexPathForItem:selectedIndex
                                                  inSection:0]
                     animated:NO
               scrollPosition:UICollectionViewScrollPositionNone];
    _initialSelectedColorID = nil;
  }
}

@end
