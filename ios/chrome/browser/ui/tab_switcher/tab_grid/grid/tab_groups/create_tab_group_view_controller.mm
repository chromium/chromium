// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/create_tab_group_view_controller.h"

#import "base/check.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "components/tab_groups/tab_group_color.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/top_aligned_image_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/keyboard/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/group_tab_info.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/create_or_edit_tab_group_view_controller_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/group_tab_view.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_creation_mutator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_snapshots_view.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_groups_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// View constants.
const CGFloat kBackgroundAlpha = 0.6;
const CGFloat kHorizontalMargin = 32;
const CGFloat kdotAndFieldContainerMargin = 44;
const CGFloat kDotTitleSeparationMargin = 12;
const CGFloat kContainersMaxWidth = 400;

// Group color selection constants.
const CGFloat kColoredButtonSize = 24;
const CGFloat kColoredButtonContentInset = 8;
const CGFloat kColorSelectionImageSize = 13;
const CGFloat kColorListViewHeight = 44;
const CGFloat kColorListBottomMargin = 16;
const CGFloat kColoredDotSize = 21;

// Snapshot view constants.
const CGFloat kSnapshotViewRatio = 0.83;
const CGFloat kSnapshotViewMaxHeight = 190;
const CGFloat kSnapshotViewCornerRadius = 18;
const CGFloat kSnapshotViewVerticalMargin = 25;
const CGFloat kSingleSnapshotRatio = 0.75;
const CGFloat kMultipleSnapshotsRatio = 0.90;

// Group title constants
const CGFloat kTitleHorizontalMargin = 16;
const CGFloat kTitleVerticalMargin = 10;
const CGFloat kTitleBackgroundCornerRadius = 17;

// Button constants
const CGFloat kButtonsHeight = 50;
const CGFloat kButtonsMargin = 8;
const CGFloat kButtonBackgroundCornerRadius = 15;

}  // namespace

@implementation CreateTabGroupViewController {
  // Text input to name the group.
  UITextField* _tabGroupTextField;
  // List of all colored buttons.
  NSArray<UIButton*>* _colorSelectionButtons;
  // Currently selected color view represented by the dot next to the title.
  UIView* _dotView;
  // Currently selected colored button.
  UIButton* _selectedButton;
  // Default color.
  tab_groups::TabGroupColorId _defaultColor;
  // StackView which contains all bottom views.
  UIStackView* _bottomStackView;
  // List of tab group pictures.
  NSArray<GroupTabInfo*>* _tabGroupInfos;
  // Snapshots views container.
  UIView* _snapshotsContainer;
  // Tab group to edit.
  const TabGroup* _tabGroup;
  // Number of selected items.
  NSInteger _numberOfSelectedItems;
  // Title of the group.
  NSString* _title;

  // Configured view that handle the snapshots dispositions.
  TabGroupSnapshotsView* _snapshotsView;
  // Constraints for the snapshots view, depending on if we display one or
  // multiple snapshots.
  NSArray<NSLayoutConstraint*>* _singleSnapshotConstraints;
  NSArray<NSLayoutConstraint*>* _multipleSnapshotsConstraints;
}

- (instancetype)initWithTabGroup:(const TabGroup*)tabGroup {
  CHECK(IsTabGroupInGridEnabled())
      << "You should not be able to create a tab group outside the Tab Groups "
         "experiment.";
  self = [super init];
  if (self) {
    _tabGroup = tabGroup;

    [self createColorSelectionButtons];
    CHECK_NE([_colorSelectionButtons count], 0u)
        << "The available color list for tab group is empty.";
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.accessibilityIdentifier = kCreateTabGroupViewIdentifier;

  __weak CreateTabGroupViewController* weakSelf = self;
  auto selectedDefaultButtonTest =
      ^BOOL(UIButton* button, NSUInteger index, BOOL* stop) {
        return [weakSelf isDefaultButton:button];
      };

  NSInteger defaultButtonIndex = [_colorSelectionButtons
      indexOfObjectPassingTest:selectedDefaultButtonTest];

  _selectedButton = _colorSelectionButtons[defaultButtonIndex];
  [_selectedButton setSelected:YES];

  [self updateViews];

  if (@available(iOS 17, *)) {
    [self registerForTraitChanges:@[ UITraitVerticalSizeClass.self ]
                       withAction:@selector(updateViews)];
  }
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(hideSnapshotsIfNeeded)
             name:UIKeyboardDidShowNotification
           object:nil];
  // To force display the keyboard when the view is shown.
  [_tabGroupTextField becomeFirstResponder];
}

- (UIStatusBarStyle)preferredStatusBarStyle {
  return UIStatusBarStyleLightContent;
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }
  if (self.traitCollection.verticalSizeClass !=
      previousTraitCollection.verticalSizeClass) {
    [self updateViews];
  }
}

#pragma mark - Private helpers

// Configures the text input dedicated for the group name.
- (UITextField*)configuredTabGroupNameTextFieldInput {
  UITextField* tabGroupTextField = [[UITextField alloc] init];
  tabGroupTextField.textColor = [UIColor colorNamed:kSolidBlackColor];
  tabGroupTextField.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleLargeTitle];
  tabGroupTextField.adjustsFontForContentSizeCategory = YES;
  tabGroupTextField.translatesAutoresizingMaskIntoConstraints = NO;
  tabGroupTextField.autocorrectionType = UITextAutocorrectionTypeNo;
  tabGroupTextField.spellCheckingType = UITextSpellCheckingTypeNo;
  tabGroupTextField.accessibilityIdentifier =
      kCreateTabGroupTextFieldIdentifier;
  tabGroupTextField.text = _title;

  [tabGroupTextField addTarget:self
                        action:@selector(creationButtonTapped)
              forControlEvents:UIControlEventPrimaryActionTriggered];

  UIColor* placeholderTextColor = [UIColor colorNamed:kTextSecondaryColor];

  tabGroupTextField.attributedPlaceholder = [[NSAttributedString alloc]
      initWithString:l10n_util::GetNSString(
                         IDS_IOS_TAB_GROUP_CREATION_PLACEHOLDER)
          attributes:@{NSForegroundColorAttributeName : placeholderTextColor}];
  return tabGroupTextField;
}

// Returns the group color dot view.
- (UIView*)groupDotViewWithColor:(UIColor*)color {
  UIView* dotView = [[UIView alloc] initWithFrame:CGRectZero];
  dotView.translatesAutoresizingMaskIntoConstraints = NO;
  dotView.layer.cornerRadius = kColoredDotSize / 2;
  dotView.backgroundColor = color;

  [NSLayoutConstraint activateConstraints:@[
    [dotView.heightAnchor constraintEqualToConstant:kColoredDotSize],
    [dotView.widthAnchor constraintEqualToConstant:kColoredDotSize],
  ]];

  return dotView;
}

// Returns the configured full primary title (colored dot and text title).
- (UIView*)configuredDotAndFieldContainer {
  UIView* titleBackground = [[UIView alloc] initWithFrame:CGRectZero];
  titleBackground.translatesAutoresizingMaskIntoConstraints = NO;
  titleBackground.backgroundColor = [UIColor colorWithWhite:1 alpha:0.1];
  titleBackground.layer.cornerRadius = kTitleBackgroundCornerRadius;
  titleBackground.opaque = NO;

  tab_groups::TabGroupColorId colorID =
      static_cast<tab_groups::TabGroupColorId>(_selectedButton.tag);

  UIColor* defaultColor = TabGroup::ColorForTabGroupColorId(colorID);
  _dotView = [self groupDotViewWithColor:defaultColor];
  _tabGroupTextField = [self configuredTabGroupNameTextFieldInput];

  [titleBackground addSubview:_dotView];
  [titleBackground addSubview:_tabGroupTextField];

  [NSLayoutConstraint activateConstraints:@[
    [_tabGroupTextField.leadingAnchor
        constraintEqualToAnchor:_dotView.trailingAnchor
                       constant:kDotTitleSeparationMargin],
    [_dotView.centerYAnchor
        constraintEqualToAnchor:_tabGroupTextField.centerYAnchor],
    [_dotView.leadingAnchor
        constraintEqualToAnchor:titleBackground.leadingAnchor
                       constant:kTitleHorizontalMargin],
    [titleBackground.trailingAnchor
        constraintEqualToAnchor:_tabGroupTextField.trailingAnchor
                       constant:kTitleHorizontalMargin],
    [_tabGroupTextField.topAnchor
        constraintEqualToAnchor:titleBackground.topAnchor
                       constant:kTitleVerticalMargin],
    [titleBackground.bottomAnchor
        constraintEqualToAnchor:_tabGroupTextField.bottomAnchor
                       constant:kTitleVerticalMargin],
  ]];
  return titleBackground;
}

// Returns the cancel button.
- (UIButton*)configuredCancelButton:(BOOL)isCompact {
  UIButton* cancelButton = [[UIButton alloc] init];
  cancelButton.translatesAutoresizingMaskIntoConstraints = NO;

  UIColor* textColor = isCompact ? [UIColor colorNamed:kBlue600Color]
                                 : [UIColor colorNamed:kSolidBlackColor];

  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  NSDictionary* attributes = @{
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleBody],
    NSForegroundColorAttributeName : textColor
  };
  NSMutableAttributedString* attributedString =
      [[NSMutableAttributedString alloc]
          initWithString:l10n_util::GetNSString(IDS_CANCEL)
              attributes:attributes];
  buttonConfiguration.attributedTitle = attributedString;

  cancelButton.configuration = buttonConfiguration;
  cancelButton.accessibilityIdentifier = kCreateTabGroupCancelButtonIdentifier;
  [cancelButton addTarget:self
                   action:@selector(cancelButtonTapped)
         forControlEvents:UIControlEventTouchUpInside];

  [NSLayoutConstraint activateConstraints:@[
    [cancelButton.heightAnchor constraintEqualToConstant:kButtonsHeight],
  ]];

  return cancelButton;
}

// Returns the cancel button.
- (UIButton*)configuredCreateGroupButton:(BOOL)isCompact {
  UIButton* creationButton = [[UIButton alloc] init];
  creationButton.translatesAutoresizingMaskIntoConstraints = NO;

  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration filledButtonConfiguration];
  if (isCompact) {
    buttonConfiguration.baseBackgroundColor = [UIColor clearColor];
  } else {
    buttonConfiguration.baseBackgroundColor =
        [UIColor colorNamed:kBlue600Color];
  }
  buttonConfiguration.background.cornerRadius = kButtonBackgroundCornerRadius;

  UIColor* textColor = isCompact ? [UIColor colorNamed:kBlue600Color]
                                 : [UIColor colorNamed:kSolidWhiteColor];
  UIFontDescriptor* boldDescriptor = [[UIFontDescriptor
      preferredFontDescriptorWithTextStyle:UIFontTextStyleBody]
      fontDescriptorWithSymbolicTraits:UIFontDescriptorTraitBold];
  UIFont* fontAttribute = [UIFont fontWithDescriptor:boldDescriptor size:0.0];
  NSDictionary* attributes = @{
    NSFontAttributeName : fontAttribute,
    NSForegroundColorAttributeName : textColor
  };
  NSMutableAttributedString* attributedString =
      [[NSMutableAttributedString alloc]
          initWithString:_tabGroup ? l10n_util::GetNSString(
                                         IDS_IOS_TAB_GROUP_CREATION_DONE)
                                   : l10n_util::GetNSString(
                                         IDS_IOS_TAB_GROUP_CREATION_BUTTON)
              attributes:attributes];
  buttonConfiguration.attributedTitle = attributedString;

  creationButton.configuration = buttonConfiguration;
  creationButton.accessibilityIdentifier =
      kCreateTabGroupCreateButtonIdentifier;
  [creationButton addTarget:self
                     action:@selector(creationButtonTapped)
           forControlEvents:UIControlEventTouchUpInside];

  [NSLayoutConstraint activateConstraints:@[
    [creationButton.heightAnchor constraintEqualToConstant:kButtonsHeight],
  ]];

  return creationButton;
}

// Hides the current view without doing anything else.
- (void)cancelButtonTapped {
  if (_tabGroup) {
    base::RecordAction(
        base::UserMetricsAction("MobileTabGroupUserCanceledGroupEdition"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("MobileTabGroupUserCanceledNewGroupCreation"));
  }
  [self dismissViewController];
}

// Creates the group and dismiss the view.
- (void)creationButtonTapped {
  __weak CreateTabGroupViewController* weakSelf = self;
  [self.mutator
      createNewGroupWithTitle:_tabGroupTextField.text
                        color:static_cast<tab_groups::TabGroupColorId>(
                                  _selectedButton.tag)
                   completion:^{
                     [weakSelf dismissViewController];
                   }];
}

// Dismisses the current view.
- (void)dismissViewController {
  // Hide the stack view before dismissing the view. The keyboard dismissing
  // animation is longer than the view one, and element attached to the keyboard
  // are still visible for a frame after the end of the view animation.
  _bottomStackView.hidden = YES;
  [self.delegate createOrEditTabGroupViewControllerDidDismiss:self];
}

// Changes the selected color.
- (void)coloredButtonTapped:(UIButton*)sender {
  if (sender.isSelected) {
    return;
  }
  [_selectedButton setSelected:NO];
  _selectedButton = sender;
  [_selectedButton setSelected:YES];
  tab_groups::TabGroupColorId colorID =
      static_cast<tab_groups::TabGroupColorId>(_selectedButton.tag);
  [_dotView setBackgroundColor:TabGroup::ColorForTabGroupColorId(colorID)];
}

// Creates all the available color buttons.
- (void)createColorSelectionButtons {
  NSMutableArray* buttons = [[NSMutableArray alloc] init];
  const tab_groups::ColorLabelMap colorLabelMap =
      tab_groups::GetTabGroupColorLabelMap();

  for (tab_groups::TabGroupColorId colorID :
       TabGroup::AllPossibleTabGroupColors()) {
    UIButton* colorButton = [[UIButton alloc] init];
    colorButton.translatesAutoresizingMaskIntoConstraints = NO;
    [colorButton setTag:static_cast<NSInteger>(colorID)];

    UIButtonConfiguration* buttonConfiguration =
        [UIButtonConfiguration plainButtonConfiguration];
    buttonConfiguration.baseBackgroundColor = [UIColor clearColor];
    buttonConfiguration.contentInsets = NSDirectionalEdgeInsets(
        kColoredButtonContentInset, kColoredButtonContentInset,
        kColoredButtonContentInset, kColoredButtonContentInset);
    colorButton.configuration = buttonConfiguration;
    colorButton.accessibilityLabel =
        base::SysUTF16ToNSString(colorLabelMap.at(colorID));

    UIImageSymbolConfiguration* configuration = [UIImageSymbolConfiguration
        configurationWithPointSize:kColoredButtonSize
                            weight:UIImageSymbolWeightRegular
                             scale:UIImageSymbolScaleDefault];
    UIImage* baseImage =
        DefaultSymbolWithConfiguration(kCircleFillSymbol, configuration);
    baseImage =
        [baseImage imageWithTintColor:TabGroup::ColorForTabGroupColorId(colorID)
                        renderingMode:UIImageRenderingModeAlwaysOriginal];

    UIImageSymbolConfiguration* selectionConfiguration =
        [UIImageSymbolConfiguration
            configurationWithPointSize:kColorSelectionImageSize
                                weight:UIImageSymbolWeightBold
                                 scale:UIImageSymbolScaleLarge];
    UIImage* selectionRingImage =
        DefaultSymbolWithConfiguration(kCircleSymbol, selectionConfiguration);
    selectionRingImage = [selectionRingImage
        imageWithTintColor:[UIColor colorNamed:kGrey100Color]
             renderingMode:UIImageRenderingModeAlwaysOriginal];
    UIGraphicsBeginImageContextWithOptions(baseImage.size, NO, 0.0f);
    [baseImage drawInRect:CGRectMake(0, 0, baseImage.size.width,
                                     baseImage.size.height)];
    [selectionRingImage
        drawInRect:CGRectMake(baseImage.size.width / 2 -
                                  selectionRingImage.size.width / 2,
                              baseImage.size.height / 2 -
                                  selectionRingImage.size.height / 2,
                              selectionRingImage.size.width,
                              selectionRingImage.size.height)];
    UIImage* selectionImage = UIGraphicsGetImageFromCurrentImageContext();
    UIGraphicsEndImageContext();

    [colorButton setImage:baseImage forState:UIControlStateNormal];
    [colorButton setImage:selectionImage forState:UIControlStateSelected];

    [colorButton addTarget:self
                    action:@selector(coloredButtonTapped:)
          forControlEvents:UIControlEventTouchUpInside];

    [buttons addObject:colorButton];
  }

  _colorSelectionButtons = buttons;
}

// Returns the configured view, which contains all the available colors.
- (UIView*)listOfColorView {
  UIStackView* colorsView = [[UIStackView alloc] init];
  colorsView.distribution = UIStackViewDistributionEqualSpacing;
  colorsView.alignment = UIStackViewAlignmentCenter;
  colorsView.translatesAutoresizingMaskIntoConstraints = NO;

  UIScrollView* scrollView = [[UIScrollView alloc] init];
  scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  scrollView.canCancelContentTouches = YES;
  [scrollView addSubview:colorsView];

  for (UIButton* button in _colorSelectionButtons) {
    [colorsView addArrangedSubview:button];
  }

  AddSameConstraints(colorsView, scrollView);
  [NSLayoutConstraint activateConstraints:@[
    [colorsView.heightAnchor constraintEqualToConstant:kColorListViewHeight],
    [scrollView.heightAnchor constraintEqualToConstant:kColorListViewHeight],
  ]];

  return scrollView;
}

// YES if the given button is the default one.
- (BOOL)isDefaultButton:(UIButton*)button {
  return static_cast<tab_groups::TabGroupColorId>(button.tag) == _defaultColor;
}

// Updates all the view and subviews depending on space available.
- (void)updateViews {
  [self.view.subviews
      makeObjectsPerformSelector:@selector(removeFromSuperview)];
  [self setupBackground];
  BOOL isVerticallyCompacted =
      self.traitCollection.verticalSizeClass == UIUserInterfaceSizeClassCompact;
  if (isVerticallyCompacted) {
    [self compactConfiguration];
  } else {
    [self regularConfiguration];
  }
  [self.view layoutIfNeeded];
  [self hideSnapshotsIfNeeded];
}

// Hides the snapshots container if it is too small.
- (void)hideSnapshotsIfNeeded {
  if (_snapshotsContainer.frame.size.height < 60) {
    [_snapshotsContainer setHidden:YES];
  } else {
    [_snapshotsContainer setHidden:NO];
  }
}

// Configures the view and all subviews when there is enough space.
- (void)regularConfiguration {
  UIView* dotAndFieldContainer = [self configuredDotAndFieldContainer];
  UILayoutGuide* snapshotsContainerLayoutGuide = [[UILayoutGuide alloc] init];
  _snapshotsContainer = [self configuredSnapshotsContainer];
  UIView* colorsView = [self listOfColorView];
  UIButton* creationButton = [self configuredCreateGroupButton:NO];
  UIButton* cancelButton = [self configuredCancelButton:NO];

  _bottomStackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ colorsView, creationButton, cancelButton ]];
  _bottomStackView.translatesAutoresizingMaskIntoConstraints = NO;
  _bottomStackView.axis = UILayoutConstraintAxisVertical;
  [_bottomStackView setCustomSpacing:kColorListBottomMargin
                           afterView:colorsView];

  [self.view addSubview:dotAndFieldContainer];
  [self.view addSubview:_snapshotsContainer];
  [self.view addLayoutGuide:snapshotsContainerLayoutGuide];
  [self.view addSubview:_bottomStackView];

  [NSLayoutConstraint activateConstraints:@[
    [dotAndFieldContainer.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:self.view.safeAreaLayoutGuide
                                                 .leadingAnchor
                                    constant:kHorizontalMargin],
    [dotAndFieldContainer.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor
                       constant:kdotAndFieldContainerMargin],
    [dotAndFieldContainer.trailingAnchor
        constraintLessThanOrEqualToAnchor:self.view.safeAreaLayoutGuide
                                              .trailingAnchor
                                 constant:-kHorizontalMargin],
    [dotAndFieldContainer.heightAnchor
        constraintGreaterThanOrEqualToConstant:kButtonsHeight],
    [_bottomStackView.bottomAnchor
        constraintEqualToAnchor:self.view.keyboardLayoutGuide.topAnchor
                       constant:-kButtonsMargin],
    [_bottomStackView.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:self.view.safeAreaLayoutGuide
                                                 .leadingAnchor
                                    constant:kHorizontalMargin],
    [_bottomStackView.trailingAnchor
        constraintLessThanOrEqualToAnchor:self.view.safeAreaLayoutGuide
                                              .trailingAnchor
                                 constant:-kHorizontalMargin],
    [snapshotsContainerLayoutGuide.topAnchor
        constraintEqualToAnchor:dotAndFieldContainer.bottomAnchor
                       constant:kSnapshotViewVerticalMargin],
    [snapshotsContainerLayoutGuide.bottomAnchor
        constraintEqualToAnchor:colorsView.topAnchor
                       constant:-kSnapshotViewVerticalMargin],
    [snapshotsContainerLayoutGuide.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:self.view.safeAreaLayoutGuide
                                                 .leadingAnchor
                                    constant:kHorizontalMargin],
    [snapshotsContainerLayoutGuide.trailingAnchor
        constraintLessThanOrEqualToAnchor:self.view.safeAreaLayoutGuide
                                              .trailingAnchor
                                 constant:-kHorizontalMargin],

    [_snapshotsContainer.centerXAnchor
        constraintEqualToAnchor:snapshotsContainerLayoutGuide.centerXAnchor],
    [_snapshotsContainer.centerYAnchor
        constraintEqualToAnchor:snapshotsContainerLayoutGuide.centerYAnchor],
    [_snapshotsContainer.topAnchor
        constraintGreaterThanOrEqualToAnchor:snapshotsContainerLayoutGuide
                                                 .topAnchor],
    [_snapshotsContainer.bottomAnchor
        constraintLessThanOrEqualToAnchor:snapshotsContainerLayoutGuide
                                              .bottomAnchor],
    [dotAndFieldContainer.widthAnchor
        constraintLessThanOrEqualToConstant:kContainersMaxWidth],
    [snapshotsContainerLayoutGuide.widthAnchor
        constraintEqualToAnchor:dotAndFieldContainer.widthAnchor],
    [_bottomStackView.widthAnchor
        constraintEqualToAnchor:dotAndFieldContainer.widthAnchor],

    [dotAndFieldContainer.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [snapshotsContainerLayoutGuide.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [_bottomStackView.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
  ]];
}

// Configures the view and subviews when the screen is small.
- (void)compactConfiguration {
  UIView* dotAndFieldContainer = [self configuredDotAndFieldContainer];
  UIView* colorsView = [self listOfColorView];
  UIButton* creationButton = [self configuredCreateGroupButton:YES];
  UIButton* cancelButton = [self configuredCancelButton:YES];

  UIStackView* containedStackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ dotAndFieldContainer, colorsView ]];
  containedStackView.translatesAutoresizingMaskIntoConstraints = NO;
  containedStackView.axis = UILayoutConstraintAxisVertical;
  containedStackView.distribution = UIStackViewDistributionEqualSpacing;

  _bottomStackView = [[UIStackView alloc] initWithArrangedSubviews:@[
    cancelButton, containedStackView, creationButton
  ]];
  _bottomStackView.translatesAutoresizingMaskIntoConstraints = NO;
  _bottomStackView.distribution = UIStackViewDistributionEqualSpacing;
  _bottomStackView.alignment = UIStackViewAlignmentTop;

  [self.view addSubview:_bottomStackView];

  [NSLayoutConstraint activateConstraints:@[
    [_bottomStackView.leadingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor
                       constant:kHorizontalMargin],
    [_bottomStackView.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor
                       constant:kdotAndFieldContainerMargin],
    [_bottomStackView.trailingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor
                       constant:-kHorizontalMargin],
    [_bottomStackView.bottomAnchor
        constraintEqualToAnchor:self.view.keyboardLayoutGuide.topAnchor
                       constant:-kButtonsMargin],
    [containedStackView.bottomAnchor
        constraintEqualToAnchor:_bottomStackView.bottomAnchor],

    [dotAndFieldContainer.widthAnchor
        constraintLessThanOrEqualToConstant:kContainersMaxWidth],
    [containedStackView.widthAnchor
        constraintEqualToAnchor:dotAndFieldContainer.widthAnchor],
  ]];
}

// Configures the view background.
- (void)setupBackground {
  if (!UIAccessibilityIsReduceTransparencyEnabled()) {
    self.view.backgroundColor = [[UIColor colorNamed:kGrey900Color]
        colorWithAlphaComponent:kBackgroundAlpha];
    UIBlurEffect* blurEffect =
        [UIBlurEffect effectWithStyle:UIBlurEffectStyleRegular];
    UIVisualEffectView* blurEffectView =
        [[UIVisualEffectView alloc] initWithEffect:blurEffect];
    blurEffectView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:blurEffectView];
    AddSameConstraints(self.view, blurEffectView);
  } else {
    self.view.backgroundColor = [UIColor blackColor];
  }
}

// Returns the view which contains all the selected tabs' snapshot which will be
// included in the tab group.
- (UIView*)configuredSnapshotsContainer {
  UIView* snapshotsBackground = [[UIView alloc] init];
  snapshotsBackground.translatesAutoresizingMaskIntoConstraints = NO;
  snapshotsBackground.backgroundColor = [UIColor colorWithWhite:1 alpha:0.1];
  snapshotsBackground.layer.cornerRadius = kSnapshotViewCornerRadius;
  snapshotsBackground.opaque = NO;

  _snapshotsView = [[TabGroupSnapshotsView alloc]
      initWithTabGroupInfos:_tabGroupInfos
                       size:_numberOfSelectedItems
                      light:self.traitCollection.userInterfaceStyle ==
                            UIUserInterfaceStyleLight
                       cell:NO];

  [snapshotsBackground addSubview:_snapshotsView];

  NSLayoutConstraint* backgroundHeightConstraint =
      [snapshotsBackground.heightAnchor
          constraintEqualToConstant:kSnapshotViewMaxHeight];
  // Lower the priority of the constraint so for smaller device, snapshot are
  // reduced instead of other elements where the user can interact with.
  backgroundHeightConstraint.priority = UILayoutPriorityDefaultLow;

  _singleSnapshotConstraints = @[
    [_snapshotsView.widthAnchor
        constraintEqualToAnchor:snapshotsBackground.widthAnchor
                     multiplier:kSingleSnapshotRatio],
    [_snapshotsView.heightAnchor
        constraintEqualToAnchor:snapshotsBackground.heightAnchor
                     multiplier:kSingleSnapshotRatio]
  ];
  _multipleSnapshotsConstraints = @[
    [_snapshotsView.widthAnchor
        constraintEqualToAnchor:snapshotsBackground.widthAnchor
                     multiplier:kMultipleSnapshotsRatio],
    [_snapshotsView.heightAnchor
        constraintEqualToAnchor:snapshotsBackground.heightAnchor
                     multiplier:kMultipleSnapshotsRatio]
  ];

  [NSLayoutConstraint activateConstraints:@[
    backgroundHeightConstraint,
    [snapshotsBackground.widthAnchor
        constraintEqualToAnchor:snapshotsBackground.heightAnchor
                     multiplier:kSnapshotViewRatio],
    [_snapshotsView.centerXAnchor
        constraintEqualToAnchor:snapshotsBackground.centerXAnchor],
    [_snapshotsView.centerYAnchor
        constraintEqualToAnchor:snapshotsBackground.centerYAnchor],
  ]];
  [self applyConstraints];

  return snapshotsBackground;
}

#pragma mark - TabGroupCreationConsumer

- (void)setDefaultGroupColor:(tab_groups::TabGroupColorId)color {
  _defaultColor = color;
}

- (void)setTabGroupInfos:(NSArray<GroupTabInfo*>*)tabGroupInfos
    numberOfSelectedItems:(NSInteger)numberOfSelectedItems {
  _tabGroupInfos = tabGroupInfos;
  _numberOfSelectedItems = numberOfSelectedItems;
  [_snapshotsView
      configureTabGroupSnapshotsViewWithTabGroupInfos:tabGroupInfos
                                                 size:_numberOfSelectedItems];
  [self applyConstraints];
}

- (void)setGroupTitle:(NSString*)title {
  _title = title;
}

#pragma mark - Accessibility

- (BOOL)accessibilityPerformEscape {
  [self dismissViewController];
  return YES;
}

#pragma mark - UIResponder

// To always be able to register key commands via -keyCommands, the VC must be
// able to become first responder.
- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (NSArray*)keyCommands {
  return @[ UIKeyCommand.cr_close ];
}

- (void)keyCommand_close {
  base::RecordAction(base::UserMetricsAction("MobileKeyCommandClose"));
  [self dismissViewController];
}

#pragma mark - Private Helpers

// Activates or deactivates the appropriate constraints.
- (void)applyConstraints {
  if (_numberOfSelectedItems == 1) {
    [NSLayoutConstraint deactivateConstraints:_multipleSnapshotsConstraints];
    [NSLayoutConstraint activateConstraints:_singleSnapshotConstraints];
  } else {
    [NSLayoutConstraint deactivateConstraints:_singleSnapshotConstraints];
    [NSLayoutConstraint activateConstraints:_multipleSnapshotsConstraints];
  }
}

@end
