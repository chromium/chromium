// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/create_tab_group_view_controller.h"

#import "base/check.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "components/tab_groups/tab_group_color.h"
#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/top_aligned_image_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/group_tab_info.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/create_or_edit_tab_group_view_controller_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/group_tab_view.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_group_creation_mutator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_group_snapshots_view.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// All elements.
const CGFloat kMaxHeight = 600;

// View constants.
const CGFloat kHorizontalMargin = 32;
const CGFloat kDotAndFieldContainerMargin = 24;
const CGFloat kDotTitleSeparationMargin = 12;
const CGFloat kContainersMaxWidth = 400;
const CGFloat kBackgroundAlpha = 0.7;
const CGFloat kCompactButtonTopMargin = 12;
const CGFloat kDotAndFieldContainerWidthPercentage = 0.5;

// Group color selection constants.
const CGFloat kColoredButtonSize = 24;
const CGFloat kColoredButtonContentInset = 8;
const CGFloat kColoredButtonWidthAndHeight = 40;
const CGFloat kColorListBottomMargin = 16;
const CGFloat kColorListBottomMarginCompact = 8;
const CGFloat kColoredDotSize = 21;

// Snapshot view constants.
const CGFloat kSnapshotViewRatio = 0.83;
const CGFloat kSnapshotViewMaxHeight = 190;
const CGFloat kSnapshotViewCornerRadius = 18;
const CGFloat kSnapshotViewVerticalMargin = 24;
const CGFloat kSingleSnapshotRatio = 0.7;
const CGFloat kMultipleSnapshotsRatio = 0.90;
const CGFloat kSnapshotViewAnimationTime = 0.3;

// Group title constants.
const CGFloat kTitleHorizontalMargin = 16;
const CGFloat kTitleVerticalMargin = 10;
const CGFloat kTitleBackgroundCornerRadius = 17;

// Button constants.
const CGFloat kButtonsHeight = 50;
const CGFloat kButtonsMargin = 8;
const CGFloat kButtonBackgroundCornerRadius = 15;

// Clear button constants.
const CGFloat kClearButtonAlpha = 0.34;
const CGFloat kClearButtonSize = 20;
const CGFloat kClearButtonWidthAndHeight = 40;

}  // namespace

// Container view for the containing the snapshot. This is here to hide itself
// if it becomes too small or in compact height. The ViewController
// callbacks aren't triggered when the keyboard is shown, so it is
// necessary to subclass UIView.
@interface CreateTabGroupSnapshotContainerView : UIView
@end

@implementation CreateTabGroupSnapshotContainerView

- (void)layoutSubviews {
  [super layoutSubviews];
  BOOL tooSmall = self.frame.size.height < 100;
  BOOL isVerticallyCompacted =
      self.traitCollection.verticalSizeClass == UIUserInterfaceSizeClassCompact;

  // The snapshots container should not be displayed in the following
  // scenarios:
  // - When the container lacks sufficient space.
  // - On devices with a vertically compact form factor.
  CGFloat updatedAlpha = (tooSmall || isVerticallyCompacted) ? 0 : 1;
  if (self.alpha == updatedAlpha) {
    return;
  }

  self.alpha = updatedAlpha;
}

@end

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
  // List of tab group pictures.
  NSArray<GroupTabInfo*>* _tabGroupInfos;
  // Snapshots views container.
  UIView* _snapshotsContainer;
  // Whether it is to edit a group (vs creation).
  BOOL _editMode;
  // Whether the user is syncing tabs.
  BOOL _tabSynced;
  // Number of selected items.
  NSInteger _numberOfSelectedItems;
  // Title of the group.
  NSString* _title;

  // Configured view that handle the snapshots dispositions.
  TabGroupSnapshotsView* _snapshotsView;
  // Constraints for the `_snapshotsView`, depending on if multiple snapshots
  // are displayed.
  NSArray<NSLayoutConstraint*>* _singleSnapshotConstraints;
  NSArray<NSLayoutConstraint*>* _multipleSnapshotsConstraints;

  // Buttons to create or cancel the group creation.
  UIButton* _creationButton;
  UIButton* _cancelButton;
  UIButton* _creationButtonCompact;
  UIButton* _cancelButtonCompact;

  // Constraints for portrait or landscape mode.
  NSArray<NSLayoutConstraint*>* _compactConstraints;
  NSArray<NSLayoutConstraint*>* _regularConstraints;

  // Scrollview that containts color selection buttons.
  UIScrollView* _colorsScrollView;
}

- (instancetype)initWithEditMode:(BOOL)editMode
                       tabSynced:(BOOL)tabSynced {
  CHECK(IsTabGroupInGridEnabled())
      << "You should not be able to create a tab group outside the Tab Groups "
         "experiment.";
  self = [super init];
  if (self) {
    _editMode = editMode;
    _tabSynced = tabSynced;

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

  [self createConfigurations];
  [self updateViews:self.view previousTraitCollection:nil];

  if (@available(iOS 17, *)) {
    [self registerForTraitChanges:@[ UITraitVerticalSizeClass.self ]
                       withAction:@selector(updateViews:
                                      previousTraitCollection:)];
  }

  // To force display the keyboard when the view is shown.
  [_tabGroupTextField becomeFirstResponder];
}

- (UIStatusBarStyle)preferredStatusBarStyle {
  return UIStatusBarStyleLightContent;
}

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }
  if (self.traitCollection.verticalSizeClass !=
      previousTraitCollection.verticalSizeClass) {
    [self updateViews:self.view
        previousTraitCollection:previousTraitCollection];
  }
}
#endif

#pragma mark - Private helpers

// Configures the text input dedicated for the group name.
- (UITextField*)configuredTabGroupNameTextFieldInput {
  UITextField* tabGroupTextField = [[UITextField alloc] init];
  tabGroupTextField.textColor = [UIColor colorNamed:kSolidBlackColor];
  tabGroupTextField.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleTitle1];
  tabGroupTextField.adjustsFontForContentSizeCategory = YES;
  tabGroupTextField.translatesAutoresizingMaskIntoConstraints = NO;
  tabGroupTextField.autocorrectionType = UITextAutocorrectionTypeNo;
  tabGroupTextField.spellCheckingType = UITextSpellCheckingTypeNo;
  tabGroupTextField.maximumContentSizeCategory =
      UIContentSizeCategoryAccessibilityExtraLarge;

  UIButton* clearButton = [UIButton buttonWithType:UIButtonTypeSystem];
  clearButton.translatesAutoresizingMaskIntoConstraints = NO;
  [clearButton setImage:DefaultSymbolWithPointSize(kXMarkCircleFillSymbol,
                                                   kClearButtonSize)
               forState:UIControlStateNormal];
  [clearButton setTintColor:[[UIColor colorNamed:kSolidBlackColor]
                                colorWithAlphaComponent:kClearButtonAlpha]];
  clearButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_ACCNAME_CLEAR_TEXT);
  clearButton.accessibilityIdentifier =
      kCreateTabGroupTextFieldClearButtonIdentifier;
  [clearButton addTarget:self
                  action:@selector(clearTextField)
        forControlEvents:UIControlEventTouchUpInside];

  NSLayoutConstraint* buttonWidth = [clearButton.widthAnchor
      constraintEqualToConstant:kClearButtonWidthAndHeight];
  buttonWidth.priority = UILayoutPriorityDefaultHigh;

  [NSLayoutConstraint activateConstraints:@[
    buttonWidth,
    [clearButton.heightAnchor constraintEqualToAnchor:clearButton.widthAnchor],
  ]];

  // Assign the overlay button to the text field
  tabGroupTextField.rightView = clearButton;
  tabGroupTextField.rightViewMode = UITextFieldViewModeWhileEditing;

  tabGroupTextField.accessibilityIdentifier =
      kCreateTabGroupTextFieldIdentifier;
  tabGroupTextField.text = _title;

  [tabGroupTextField addTarget:self
                        action:@selector(creationButtonTapped)
              forControlEvents:UIControlEventPrimaryActionTriggered];
  [tabGroupTextField addTarget:self
                        action:@selector(textFieldDidChange)
              forControlEvents:UIControlEventEditingChanged];

  UIColor* placeholderTextColor = [UIColor colorNamed:kTextSecondaryColor];

  tabGroupTextField.attributedPlaceholder = [[NSAttributedString alloc]
      initWithString:l10n_util::GetNSString(
                         IDS_IOS_TAB_GROUP_CREATION_PLACEHOLDER)
          attributes:@{NSForegroundColorAttributeName : placeholderTextColor}];

  return tabGroupTextField;
}

// Removes text in the text field.
- (void)clearTextField {
  _tabGroupTextField.text = @"";
  _tabGroupTextField.rightView.hidden = YES;
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
  titleBackground.backgroundColor = [[UIColor colorNamed:kSolidWhiteColor]
      colorWithAlphaComponent:kBackgroundAlpha];
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
- (UIButton*)configuredCancelButtonCompacted:(BOOL)isCompact {
  UIButton* cancelButton = [UIButton buttonWithType:UIButtonTypeSystem];
  cancelButton.translatesAutoresizingMaskIntoConstraints = NO;

  UIColor* textColor = isCompact ? [UIColor colorNamed:kBlue600Color]
                                 : [UIColor colorNamed:kSolidBlackColor];

  NSDictionary* attributes = @{
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleBody],
    NSForegroundColorAttributeName : textColor
  };
  NSMutableAttributedString* attributedString =
      [[NSMutableAttributedString alloc]
          initWithString:l10n_util::GetNSString(IDS_CANCEL)
              attributes:attributes];
  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  buttonConfiguration.attributedTitle = attributedString;
  cancelButton.configuration = buttonConfiguration;

  [cancelButton setAttributedTitle:attributedString
                          forState:UIControlStateNormal];

  cancelButton.maximumContentSizeCategory =
      UIContentSizeCategoryExtraExtraLarge;

  cancelButton.accessibilityIdentifier = kCreateTabGroupCancelButtonIdentifier;
  [cancelButton addTarget:self
                   action:@selector(cancelButtonTapped)
         forControlEvents:UIControlEventTouchUpInside];

  [NSLayoutConstraint activateConstraints:@[
    [cancelButton.heightAnchor
        constraintGreaterThanOrEqualToConstant:kButtonsHeight],
  ]];

  return cancelButton;
}

// Returns the cancel button.
- (UIButton*)configuredCreateGroupButtonCompacted:(BOOL)isCompact {
  UIButton* creationButton = [UIButton buttonWithType:UIButtonTypeSystem];
  creationButton.translatesAutoresizingMaskIntoConstraints = NO;

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
          initWithString:_editMode ? l10n_util::GetNSString(
                                         IDS_IOS_TAB_GROUP_CREATION_DONE)
                                   : l10n_util::GetNSString(
                                         IDS_IOS_TAB_GROUP_CREATION_BUTTON)
              attributes:attributes];

  if (isCompact) {
    // The compact button adheres to the style of the iOS system buttons.
    UIButtonConfiguration* buttonConfiguration =
        [UIButtonConfiguration plainButtonConfiguration];
    buttonConfiguration.titleLineBreakMode = NSLineBreakByWordWrapping;
    buttonConfiguration.attributedTitle = attributedString;
    creationButton.configuration = buttonConfiguration;
  } else {
    UIButtonConfiguration* buttonConfiguration =
        [UIButtonConfiguration filledButtonConfiguration];
    buttonConfiguration.baseBackgroundColor =
        [UIColor colorNamed:kBlue600Color];
    buttonConfiguration.background.cornerRadius = kButtonBackgroundCornerRadius;
    buttonConfiguration.attributedTitle = attributedString;
    creationButton.configuration = buttonConfiguration;
  }

  creationButton.maximumContentSizeCategory =
      UIContentSizeCategoryExtraExtraLarge;

  creationButton.accessibilityIdentifier =
      kCreateTabGroupCreateButtonIdentifier;
  [creationButton addTarget:self
                     action:@selector(creationButtonTapped)
           forControlEvents:UIControlEventTouchUpInside];

  [NSLayoutConstraint activateConstraints:@[
    [creationButton.heightAnchor
        constraintGreaterThanOrEqualToConstant:kButtonsHeight],
  ]];

  return creationButton;
}

// Hides the current view without doing anything else.
- (void)cancelButtonTapped {
  if (_editMode) {
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
  // Hide elements attached to the keyboard for dismissing the view. The
  // keyboard dismissing animation is longer than the view one, and elements
  // attached to the keyboard are still visible for a frame after the end of the
  // view animation.
  _cancelButton.hidden = YES;
  _creationButton.hidden = YES;
  _colorsScrollView.hidden = YES;
  [self.delegate createOrEditTabGroupViewControllerDidDismiss:self
                                                     animated:YES];
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
    colorButton.accessibilityLabel = l10n_util::GetNSStringF(
        IDS_IOS_TAB_GROUP_CREATION_ACCESSIBILITY_COLOR_SELECTION,
        colorLabelMap.at(colorID));

    UIImageSymbolConfiguration* configuration = [UIImageSymbolConfiguration
        configurationWithPointSize:kColoredButtonSize
                            weight:UIImageSymbolWeightRegular
                             scale:UIImageSymbolScaleDefault];

    UIImage* normalSymbolImage =
        DefaultSymbolWithConfiguration(kCircleFillSymbol, configuration);
    normalSymbolImage = [normalSymbolImage
        imageWithTintColor:TabGroup::ColorForTabGroupColorId(colorID)
             renderingMode:UIImageRenderingModeAlwaysOriginal];

    UIImage* selectedSymbolImage =
        DefaultSymbolWithConfiguration(kCircleCircleFillSymbol, configuration);
    selectedSymbolImage = [selectedSymbolImage
        imageWithTintColor:TabGroup::ColorForTabGroupColorId(colorID)
             renderingMode:UIImageRenderingModeAlwaysOriginal];

    [colorButton setImage:normalSymbolImage forState:UIControlStateNormal];
    [colorButton setImage:selectedSymbolImage forState:UIControlStateSelected];
    [colorButton addTarget:self
                    action:@selector(coloredButtonTapped:)
          forControlEvents:UIControlEventTouchUpInside];

    [NSLayoutConstraint activateConstraints:@[
      [colorButton.widthAnchor
          constraintEqualToConstant:kColoredButtonWidthAndHeight],
      [colorButton.heightAnchor
          constraintEqualToAnchor:colorButton.widthAnchor],
    ]];

    [buttons addObject:colorButton];
  }

  _colorSelectionButtons = buttons;
}

// Returns the configured view, which contains all the available colors.
- (UIScrollView*)listOfColorView {
  UIStackView* colorsView = [[UIStackView alloc] init];
  colorsView.alignment = UIStackViewAlignmentTop;
  colorsView.translatesAutoresizingMaskIntoConstraints = NO;

  UIScrollView* scrollView = [[UIScrollView alloc] init];
  scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  scrollView.canCancelContentTouches = YES;
  [scrollView setShowsHorizontalScrollIndicator:NO];
  [scrollView setShowsVerticalScrollIndicator:NO];
  [scrollView addSubview:colorsView];

  for (UIButton* button in _colorSelectionButtons) {
    [colorsView addArrangedSubview:button];
  }

  [self updateScrollViewInsets:scrollView];

  NSLayoutConstraint* scrollViewWidthConstraint = [scrollView.widthAnchor
      constraintGreaterThanOrEqualToAnchor:colorsView.widthAnchor];
  scrollViewWidthConstraint.priority = UILayoutPriorityDefaultLow;

  AddSameConstraints(scrollView.contentLayoutGuide, colorsView);
  [NSLayoutConstraint activateConstraints:@[
    [scrollView.heightAnchor constraintEqualToAnchor:colorsView.heightAnchor],
    scrollViewWidthConstraint,
  ]];

  return scrollView;
}

// YES if the given button is the default one.
- (BOOL)isDefaultButton:(UIButton*)button {
  return static_cast<tab_groups::TabGroupColorId>(button.tag) == _defaultColor;
}

// Updates all the view and subviews depending on space available.
- (void)updateViews:(UIView*)updatedView
    previousTraitCollection:(UITraitCollection*)previousTraitCollection {
  if (previousTraitCollection.verticalSizeClass ==
      self.traitCollection.verticalSizeClass) {
    return;
  }
  NSArray<UIView*>* toHide;
  NSArray<UIView*>* toDisplay;

  BOOL isVerticallyCompacted =
      self.traitCollection.verticalSizeClass == UIUserInterfaceSizeClassCompact;
  if (isVerticallyCompacted) {
    toHide = @[ _cancelButton, _creationButton ];
    toDisplay = @[ _cancelButtonCompact, _creationButtonCompact ];
    [NSLayoutConstraint deactivateConstraints:_regularConstraints];
    [NSLayoutConstraint activateConstraints:_compactConstraints];
  } else {
    toHide = @[ _cancelButtonCompact, _creationButtonCompact ];
    toDisplay = @[ _cancelButton, _creationButton ];
    [NSLayoutConstraint deactivateConstraints:_compactConstraints];
    [NSLayoutConstraint activateConstraints:_regularConstraints];
  }

  for (UIView* view in toDisplay) {
    view.hidden = NO;
    view.alpha = 0;
  }
  UIScrollView* scrollView = _colorsScrollView;
  __weak __typeof(self) weakSelf = self;
  [UIView animateWithDuration:kSnapshotViewAnimationTime
      animations:^{
        for (UIView* view in toHide) {
          view.alpha = 0;
        }
        for (UIView* view in toDisplay) {
          view.alpha = 1;
        }
        [weakSelf updateScrollViewInsets:scrollView];
        [weakSelf.view layoutIfNeeded];
      }
      completion:^(BOOL finished) {
        for (UIView* view in toHide) {
          view.hidden = YES;
        }
      }];

  // To force display the keyboard.
  [_tabGroupTextField becomeFirstResponder];
}

// Configures the view and all subviews when there is enough space.
- (void)createConfigurations {
  UIView* dotAndFieldContainer = [self configuredDotAndFieldContainer];
  UILayoutGuide* snapshotsContainerLayoutGuide = [[UILayoutGuide alloc] init];
  _snapshotsContainer = [self configuredSnapshotsContainer];
  _colorsScrollView = [self listOfColorView];
  _creationButton = [self configuredCreateGroupButtonCompacted:NO];
  _cancelButton = [self configuredCancelButtonCompacted:NO];
  _creationButtonCompact = [self configuredCreateGroupButtonCompacted:YES];
  _cancelButtonCompact = [self configuredCancelButtonCompacted:YES];

  UIView* container = [[UIView alloc] init];
  container.translatesAutoresizingMaskIntoConstraints = NO;

  [container addSubview:dotAndFieldContainer];
  [container addSubview:_snapshotsContainer];
  [container addLayoutGuide:snapshotsContainerLayoutGuide];
  [container addSubview:_colorsScrollView];
  [container addSubview:_cancelButtonCompact];
  [container addSubview:_creationButtonCompact];
  [container addSubview:_cancelButton];
  [container addSubview:_creationButton];
  [self.view addSubview:container];

  NSLayoutConstraint* keyboardConstraint = [container.bottomAnchor
      constraintEqualToAnchor:self.view.keyboardLayoutGuide.topAnchor];
  keyboardConstraint.priority = UILayoutPriorityDefaultHigh + 1;

  NSLayoutConstraint* snapshotLayoutGuideConstraint =
      [snapshotsContainerLayoutGuide.bottomAnchor
          constraintEqualToAnchor:_colorsScrollView.topAnchor
                         constant:-kSnapshotViewVerticalMargin];
  snapshotLayoutGuideConstraint.priority = UILayoutPriorityDefaultHigh + 1;

  _regularConstraints = @[
    [dotAndFieldContainer.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:container.leadingAnchor
                                    constant:kHorizontalMargin],
    [dotAndFieldContainer.trailingAnchor
        constraintLessThanOrEqualToAnchor:container.trailingAnchor
                                 constant:-kHorizontalMargin],
    [_creationButton.widthAnchor
        constraintEqualToAnchor:dotAndFieldContainer.widthAnchor],
    [_cancelButton.widthAnchor
        constraintEqualToAnchor:dotAndFieldContainer.widthAnchor],
    [_cancelButton.bottomAnchor constraintEqualToAnchor:container.bottomAnchor
                                               constant:-kButtonsMargin],
  ];

  _compactConstraints = @[
    [dotAndFieldContainer.widthAnchor
        constraintLessThanOrEqualToAnchor:self.view.widthAnchor
                               multiplier:kDotAndFieldContainerWidthPercentage],
    [_cancelButtonCompact.trailingAnchor
        constraintLessThanOrEqualToAnchor:dotAndFieldContainer.leadingAnchor],
    [_creationButtonCompact.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:dotAndFieldContainer
                                                 .trailingAnchor],
    [_colorsScrollView.bottomAnchor
        constraintEqualToAnchor:container.bottomAnchor
                       constant:-kColorListBottomMarginCompact],
  ];

  NSLayoutConstraint* dotAndFieldWidth = [dotAndFieldContainer.widthAnchor
      constraintEqualToConstant:kContainersMaxWidth];
  dotAndFieldWidth.priority = UILayoutPriorityDefaultHigh;

  [NSLayoutConstraint activateConstraints:@[
    [dotAndFieldContainer.topAnchor
        constraintEqualToAnchor:container.topAnchor
                       constant:kDotAndFieldContainerMargin],
    [dotAndFieldContainer.heightAnchor
        constraintGreaterThanOrEqualToConstant:kButtonsHeight],
    dotAndFieldWidth,
    [dotAndFieldContainer.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [_colorsScrollView.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:container.leadingAnchor],
    [_colorsScrollView.trailingAnchor
        constraintLessThanOrEqualToAnchor:container.trailingAnchor],
    [_colorsScrollView.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],

    [container.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor],
    [container.leadingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor],
    [container.trailingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor],
    [container.heightAnchor constraintLessThanOrEqualToConstant:kMaxHeight],

    [_creationButton.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [_cancelButton.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [_creationButton.bottomAnchor
        constraintEqualToAnchor:_cancelButton.topAnchor],
    [_creationButton.topAnchor
        constraintEqualToAnchor:_colorsScrollView.bottomAnchor
                       constant:kColorListBottomMargin],

    [_cancelButtonCompact.leadingAnchor
        constraintEqualToAnchor:container.leadingAnchor],
    [_creationButtonCompact.trailingAnchor
        constraintEqualToAnchor:container.trailingAnchor],
    [_cancelButtonCompact.topAnchor
        constraintEqualToAnchor:container.topAnchor
                       constant:kCompactButtonTopMargin],
    [_creationButtonCompact.topAnchor
        constraintEqualToAnchor:container.topAnchor
                       constant:kCompactButtonTopMargin],

    [snapshotsContainerLayoutGuide.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [snapshotsContainerLayoutGuide.topAnchor
        constraintEqualToAnchor:dotAndFieldContainer.bottomAnchor
                       constant:kSnapshotViewVerticalMargin],
    [snapshotsContainerLayoutGuide.widthAnchor
        constraintEqualToAnchor:dotAndFieldContainer.widthAnchor],
    snapshotLayoutGuideConstraint,

    [_snapshotsContainer.centerXAnchor
        constraintEqualToAnchor:snapshotsContainerLayoutGuide.centerXAnchor],
    [_snapshotsContainer.centerYAnchor
        constraintEqualToAnchor:snapshotsContainerLayoutGuide.centerYAnchor],
    [_snapshotsContainer.heightAnchor
        constraintLessThanOrEqualToAnchor:snapshotsContainerLayoutGuide
                                              .heightAnchor],
    [_snapshotsContainer.widthAnchor
        constraintLessThanOrEqualToAnchor:snapshotsContainerLayoutGuide
                                              .widthAnchor],
    keyboardConstraint,
  ]];
}

// Returns the view which contains all the selected tabs' snapshot which will be
// included in the tab group.
- (UIView*)configuredSnapshotsContainer {
  UIView* snapshotsBackground =
      [[CreateTabGroupSnapshotContainerView alloc] init];
  snapshotsBackground.translatesAutoresizingMaskIntoConstraints = NO;
  snapshotsBackground.backgroundColor = [[UIColor colorNamed:kSolidWhiteColor]
      colorWithAlphaComponent:kBackgroundAlpha];
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
  backgroundHeightConstraint.priority = UILayoutPriorityDefaultHigh;

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

// Hides the clear button in the text field if the length of the text is 0.
- (void)textFieldDidChange {
  BOOL hasText = _tabGroupTextField.text.length > 0;
  _tabGroupTextField.rightView.hidden = hasText ? NO : YES;
}

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

// Updates the insets of the `colorScrollView`.
- (void)updateScrollViewInsets:(UIScrollView*)colorScrollView {
  CGFloat viewWidth = self.view.safeAreaLayoutGuide.layoutFrame.size.width;
  CGFloat selectionWidth =
      [_colorSelectionButtons count] * kColoredButtonWidthAndHeight;

  if (selectionWidth > viewWidth) {
    colorScrollView.contentInset =
        UIEdgeInsetsMake(0, kHorizontalMargin, 0, kHorizontalMargin);
    if (colorScrollView.contentOffset.x == 0) {
      colorScrollView.contentOffset = CGPointMake(-kHorizontalMargin, 0);
    }
  } else {
    colorScrollView.contentInset = UIEdgeInsetsZero;
  }
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

@end
