// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/create_tab_group_view_controller.h"

#import "base/check.h"
#import "components/strings/grit/components_strings.h"
#import "components/tab_groups/tab_group_color.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/top_aligned_image_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_creation_mutator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_snapshots_view.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_groups_commands.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_groups_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
constexpr CGFloat kBackgroundAlpha = 0.6;
constexpr CGFloat kColoredDotSize = 21;
constexpr CGFloat kTitleHorizontalMargin = 16;
constexpr CGFloat kTitleVerticalMargin = 10;
constexpr CGFloat kHorizontalMargin = 32;
constexpr CGFloat kdotAndFieldContainerMargin = 44;
constexpr CGFloat kDotTitleSeparationMargin = 12;
constexpr CGFloat kTitleBackgroundCornerRadius = 17;
constexpr CGFloat kButtonsHeight = 50;
constexpr CGFloat kButtonsMargin = 8;
constexpr CGFloat kButtonBackgroundCornerRadius = 15;
constexpr CGFloat kColoredButtonSize = 24;
constexpr CGFloat kColorSelectionImageSize = 13;
constexpr CGFloat kColorListViewHeight = 44;
constexpr CGFloat kColorListBottomMargin = 16;
constexpr CGFloat kSnapshotViewRatio = 0.83;
constexpr CGFloat kSnapshotViewMaxHeight = 190;
constexpr CGFloat kSnapshotViewCornerRadius = 18;
constexpr CGFloat kSnapshotViewVerticalMargin = 25;
constexpr CGFloat kSingleSnapshotRatio = 0.75;
constexpr CGFloat kContainersMaxWidth = 400;
}  // namespace

@implementation CreateTabGroupViewController {
  // Text input to name the group.
  UITextField* _tabGroupTextField;
  // Handler to handle user's actions.
  __weak id<TabGroupsCommands> _tabGroupsHandler;
  // List of all colored buttons.
  NSArray<UIButton*>* _colorSelectionButtons;
  // Currently selected color view represented by the dot next to the title.
  UIView* _dotView;
  // Currently selected colored button.
  UIButton* _selectedButton;
  // Lists which contains all the available colors.
  NSArray<UIColor*>* _UIColorList;
  // Lists which contains all the available colors ID.
  std::vector<tab_groups::TabGroupColorId> _colorIDList;
  // Default color.
  tab_groups::TabGroupColorId _defaultColor;
  // StackView which contains all bottom views.
  UIStackView* _bottomStackView;
  // List of snapshots.
  NSArray<UIImage*>* _snapshots;
  // List of favicons.
  NSArray<UIImage*>* _favicons;
  // Snapshots views container.
  UIView* _snapshotsContainer;
}

- (instancetype)initWithHandler:(id<TabGroupsCommands>)handler {
  CHECK(base::FeatureList::IsEnabled(kTabGroupsInGrid))
      << "You should not be able to create a tab group outside the Tab Groups "
         "experiment.";
  self = [super init];
  if (self) {
    CHECK(handler);
    _tabGroupsHandler = handler;

    // TODO(crbug.com/1501837): Get the color ID list from helper to ensure to
    // always have the correct values.
    _colorIDList = {
        tab_groups::TabGroupColorId::kGrey,
        tab_groups::TabGroupColorId::kBlue,
        tab_groups::TabGroupColorId::kRed,
        tab_groups::TabGroupColorId::kYellow,
        tab_groups::TabGroupColorId::kGreen,
        tab_groups::TabGroupColorId::kPink,
        tab_groups::TabGroupColorId::kPurple,
        tab_groups::TabGroupColorId::kCyan,
        tab_groups::TabGroupColorId::kOrange,
    };

    // TODO(crbug.com/1501837): Get the color list from helper to ensure to
    // always have the correct values.
    _UIColorList = @[
      // tab_groups::TabGroupColorId::kGrey
      [UIColor colorNamed:kStaticGrey300Color],
      // tab_groups::TabGroupColorId::kBlue
      [UIColor colorNamed:kBlueColor],
      // tab_groups::TabGroupColorId::kRed
      [UIColor colorNamed:kRedColor],
      // tab_groups::TabGroupColorId::kYellow
      [UIColor colorNamed:kYellow500Color],
      // tab_groups::TabGroupColorId::kGreen
      [UIColor colorNamed:kGreenColor],
      // tab_groups::TabGroupColorId::kPink
      [UIColor colorNamed:kPink500Color],
      // tab_groups::TabGroupColorId::kPurple
      [UIColor colorNamed:kPurple500Color],
      // tab_groups::TabGroupColorId::kCyan
      [UIColor colorNamed:kBlueHaloColor],
      // tab_groups::TabGroupColorId::kOrange
      [UIColor colorNamed:kOrange500Color],
    ];

    [self createColorSelectionButtons];
    CHECK_NE([_colorSelectionButtons count], 0u)
        << "The available color list for tab group is empty.";
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.accessibilityIdentifier = kCreateTabGroupIdentifier;

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
  tabGroupTextField.textColor = [UIColor colorNamed:kSolidWhiteColor];
  tabGroupTextField.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleLargeTitle];
  tabGroupTextField.adjustsFontForContentSizeCategory = YES;
  tabGroupTextField.translatesAutoresizingMaskIntoConstraints = NO;
  tabGroupTextField.autocorrectionType = UITextAutocorrectionTypeNo;
  tabGroupTextField.spellCheckingType = UITextSpellCheckingTypeNo;

  UITraitCollection* interfaceStyleDarkTraitCollection = [UITraitCollection
      traitCollectionWithUserInterfaceStyle:UIUserInterfaceStyleDark];
  UIColor* placeholderTextColor = [[UIColor colorNamed:kTextSecondaryColor]
      resolvedColorWithTraitCollection:interfaceStyleDarkTraitCollection];

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

  UIColor* defaultColor =
      [self tabGroupColorFromColorID:static_cast<tab_groups::TabGroupColorId>(
                                         _selectedButton.tag)];
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
                                 : [UIColor colorNamed:kSolidWhiteColor];

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
  UIFont* fontAttribute =
      isCompact ? [UIFont fontWithDescriptor:boldDescriptor size:0.0]
                : [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  NSDictionary* attributes = @{
    NSFontAttributeName : fontAttribute,
    NSForegroundColorAttributeName : textColor
  };
  NSMutableAttributedString* attributedString =
      [[NSMutableAttributedString alloc]
          initWithString:l10n_util::GetNSString(
                             IDS_IOS_TAB_GROUP_CREATION_BUTTON)
              attributes:attributes];
  buttonConfiguration.attributedTitle = attributedString;

  creationButton.configuration = buttonConfiguration;

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
  [_tabGroupsHandler hideTabGroupCreation];
}

// Changes the selected color.
- (void)coloredButtonTapped:(UIButton*)sender {
  if (sender.isSelected) {
    return;
  }
  [_selectedButton setSelected:NO];
  _selectedButton = sender;
  [_selectedButton setSelected:YES];
  [_dotView
      setBackgroundColor:[self tabGroupColorFromColorID:
                                   static_cast<tab_groups::TabGroupColorId>(
                                       _selectedButton.tag)]];
}

// Creates all the available color buttons.
- (void)createColorSelectionButtons {
  NSMutableArray* buttons = [[NSMutableArray alloc] init];
  for (tab_groups::TabGroupColorId colorID : _colorIDList) {
    UIButton* colorButton = [[UIButton alloc] init];
    colorButton.translatesAutoresizingMaskIntoConstraints = NO;
    [colorButton setTag:static_cast<NSInteger>(colorID)];

    UIButtonConfiguration* buttonConfiguration =
        [UIButtonConfiguration filledButtonConfiguration];
    buttonConfiguration.baseBackgroundColor =
        [self tabGroupColorFromColorID:colorID];
    buttonConfiguration.background.cornerRadius = kColoredButtonSize / 2;
    colorButton.configuration = buttonConfiguration;

    UIImageSymbolConfiguration* configuration = [UIImageSymbolConfiguration
        configurationWithPointSize:kColorSelectionImageSize
                            weight:UIImageSymbolWeightBold
                             scale:UIImageSymbolScaleLarge];
    UIImage* image =
        DefaultSymbolWithConfiguration(kCircleSymbol, configuration);
    image = [image imageWithTintColor:[UIColor colorNamed:kGrey900Color]
                        renderingMode:UIImageRenderingModeAlwaysOriginal];
    UIImage* emptyImage = [[UIImage alloc] init];

    [colorButton setImage:emptyImage forState:UIControlStateNormal];
    [colorButton setImage:image forState:UIControlStateSelected];

    [colorButton addTarget:self
                    action:@selector(coloredButtonTapped:)
          forControlEvents:UIControlEventTouchUpInside];

    [NSLayoutConstraint activateConstraints:@[
      [colorButton.heightAnchor constraintEqualToConstant:kColoredButtonSize],
      [colorButton.widthAnchor constraintEqualToConstant:kColoredButtonSize],
    ]];

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
  // Add empty view before and after so the equal spacing distribution do not
  // put dots at view's boundary and dots stay completely inside the view.
  [colorsView addArrangedSubview:[[UIView alloc] init]];
  for (UIButton* button in _colorSelectionButtons) {
    [colorsView addArrangedSubview:button];
  }
  [colorsView addArrangedSubview:[[UIView alloc] init]];

  [NSLayoutConstraint activateConstraints:@[
    [colorsView.heightAnchor constraintEqualToConstant:kColorListViewHeight],
  ]];

  return colorsView;
}

// Color and color ID mapping.
// TODO(crbug.com/1501837): Remove once the color helper exist.
- (UIColor*)tabGroupColorFromColorID:(tab_groups::TabGroupColorId)colorID {
  return _UIColorList[static_cast<NSUInteger>(colorID)];
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
  if ([_snapshotsContainer superview] &&
      _snapshotsContainer.frame.size.height < 40) {
    [_snapshotsContainer removeFromSuperview];
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
        [UIBlurEffect effectWithStyle:UIBlurEffectStyleDark];
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

  // TODO(crbug.com/1501837): Manage more than one snapshot and favicons.
  UIView* snapshotView = [[TabGroupSnapshotsView alloc]
      initWithSnapshot:[self imageFromObject:_snapshots.firstObject]
               favicon:[self imageFromObject:_favicons.firstObject]];

  [snapshotsBackground addSubview:snapshotView];

  NSLayoutConstraint* backgroundHeightConstraint =
      [snapshotsBackground.heightAnchor
          constraintEqualToConstant:kSnapshotViewMaxHeight];
  // Lower the priority of the constraint so for smaller device, snapshot are
  // reduced instead of other elements where the user can interact with.
  backgroundHeightConstraint.priority = UILayoutPriorityDefaultLow;

  [NSLayoutConstraint activateConstraints:@[
    backgroundHeightConstraint,
    [snapshotsBackground.widthAnchor
        constraintEqualToAnchor:snapshotsBackground.heightAnchor
                     multiplier:kSnapshotViewRatio],
    [snapshotView.widthAnchor
        constraintEqualToAnchor:snapshotsBackground.widthAnchor
                     multiplier:kSingleSnapshotRatio],
    [snapshotView.heightAnchor
        constraintEqualToAnchor:snapshotsBackground.heightAnchor
                     multiplier:kSingleSnapshotRatio],
    [snapshotView.centerXAnchor
        constraintEqualToAnchor:snapshotsBackground.centerXAnchor],
    [snapshotView.centerYAnchor
        constraintEqualToAnchor:snapshotsBackground.centerYAnchor],
  ]];

  return snapshotsBackground;
}

#pragma mark - TabGroupCreationConsumer

- (void)setDefaultGroupColor:(tab_groups::TabGroupColorId)color {
  _defaultColor = color;
}

- (void)setSnapshots:(NSArray<UIImage*>*)snapshots
            favicons:(NSArray<UIImage*>*)favicons {
  _snapshots = snapshots;
  _favicons = favicons;
}

#pragma mark - Private Helpers

// Returns the picture is it is a picture and nil if not.
- (UIImage*)imageFromObject:(id)object {
  if ([object isKindOfClass:[NSNull class]]) {
    return nil;
  }
  return object;
}

@end
