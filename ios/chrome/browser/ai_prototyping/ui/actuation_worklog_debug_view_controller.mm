// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ui/actuation_worklog_debug_view_controller.h"

#import <cmath>
#import <optional>
#import <utility>

#import "base/apple/foundation_util.h"
#import "base/ios/block_types.h"
#import "base/notreached.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_constants.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

using intelligence::actor::kSpacingLarge;
using intelligence::actor::kSpacingMedium;
using intelligence::actor::kSpacingSmall;

const CGFloat kIconSize = 16.0;

// Reuse identifiers for the sandbox collection view cells.
NSString* const kWorklogCellIdentifier = @"WorklogCell";
NSString* const kResponseCellIdentifier = @"ResponseCell";

// Number of cells in the sandbox collection view (inline worklog + summary).
constexpr NSInteger kSandboxCellCount = 2;

// Mock sibling block rendered below the worklog cell to verify self-sizing.
NSString* const kMockResponseSummary =
    @"Mock response block below the inline worklog. Collapsing, expanding, "
    @"adding steps, or toggling the chip above shifts this cell.";

}  // namespace

// Replicates the Gemini SDK's `FloatyCustomContentCell.ContainerView`, which
// notifies its owning cell whenever its bounds change during layout.
@interface ActuationWorklogSandboxContainerView : UIView
@property(nonatomic, copy) ProceduralBlock onLayout;
@end

@implementation ActuationWorklogSandboxContainerView

- (void)layoutSubviews {
  [super layoutSubviews];
  if (self.onLayout) {
    self.onLayout();
  }
}

@end

// Replicates the Gemini SDK's `GMNFloatyCustomContentCell` layout and height
// invalidation contract so the sandbox exercises the exact host cell behavior.
@interface ActuationWorklogSandboxHostCell : UICollectionViewCell
@property(nonatomic, copy) ProceduralBlock hostedViewHeightDidChange;
// Hosts `view` inside the container, replacing any previously hosted view.
- (void)hostView:(UIView*)view;
@end

@implementation ActuationWorklogSandboxHostCell {
  ActuationWorklogSandboxContainerView* _container;
  __weak UIView* _lastHostedView;
  std::optional<CGFloat> _lastReportedHeight;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _container = [[ActuationWorklogSandboxContainerView alloc] init];
    _container.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:_container];
    AddSameConstraintsToSides(
        _container, self.contentView,
        LayoutSides::kTop | LayoutSides::kLeading | LayoutSides::kTrailing);
    NSLayoutConstraint* bottomConstraint = [_container.bottomAnchor
        constraintEqualToAnchor:self.contentView.bottomAnchor];
    bottomConstraint.priority = UILayoutPriorityDefaultLow - 1;
    bottomConstraint.active = YES;

    __weak __typeof(self) weakSelf = self;
    _container.onLayout = ^{
      [weakSelf handleContainerLayout];
    };
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.hostedViewHeightDidChange = nil;
  [self hostView:nil];
}

- (void)hostView:(UIView*)view {
  if (_lastHostedView == view && view.superview == _container) {
    return;
  }
  // Only remove the previous view if another cell hasn't taken it.
  if (_lastHostedView.superview == _container) {
    [_lastHostedView removeFromSuperview];
  }
  _lastHostedView = view;
  _lastReportedHeight = std::nullopt;
  if (view) {
    view.translatesAutoresizingMaskIntoConstraints = NO;
    [_container addSubview:view];
    AddSameConstraints(view, _container);
  }
  [self.contentView setNeedsLayout];
}

#pragma mark - Private

// Reports height changes when `_container` diverges from `contentView` bounds.
- (void)handleContainerLayout {
  UIView* hostedView = _lastHostedView;
  if (!hostedView || hostedView.superview != _container) {
    return;
  }
  CGFloat containerHeight = _container.bounds.size.height;
  CGFloat cellHeight = self.contentView.bounds.size.height;
  if (std::abs(containerHeight - cellHeight) < 1.0) {
    return;
  }
  if (_lastReportedHeight == containerHeight) {
    return;
  }
  _lastReportedHeight = containerHeight;
  if (self.hostedViewHeightDidChange) {
    self.hostedViewHeightDidChange();
  }
}

@end

@interface ActuationWorklogDebugViewController () <UICollectionViewDataSource,
                                                   UITextFieldDelegate> {
  UICollectionView* _collectionView;
  ActuationWorklogView* _worklogView;
  NSMutableArray<ActuationWorklogItem*>* _worklogItems;
  BOOL _worklogResizeScheduled;
  // Set while a scroll to the bottom waits for the scheduled worklog cell
  // resize; holds whether to animate it.
  std::optional<BOOL> _pendingScrollToBottomAnimated;

  UITextField* _titleField;
  UITextField* _subtitleField;
  UISegmentedControl* _stepStyleSelector;
  UISwitch* _actuatingSwitch;

  UITextField* _chipField;
  UISegmentedControl* _chipIconSelector;
}
@end

@implementation ActuationWorklogDebugViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = @"Actuation Worklog Sandbox";
  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  self.view.keyboardLayoutGuide.usesBottomSafeArea = NO;

  UICollectionLayoutListConfiguration* listConfig =
      [[UICollectionLayoutListConfiguration alloc]
          initWithAppearance:UICollectionLayoutListAppearancePlain];
  listConfig.showsSeparators = NO;
  listConfig.backgroundColor = [UIColor clearColor];
  UICollectionViewCompositionalLayout* layout =
      [UICollectionViewCompositionalLayout
          layoutWithListConfiguration:listConfig];

  _collectionView = [[UICollectionView alloc] initWithFrame:CGRectZero
                                       collectionViewLayout:layout];
  _collectionView.backgroundColor = [UIColor clearColor];
  _collectionView.dataSource = self;
  _collectionView.translatesAutoresizingMaskIntoConstraints = NO;
  [_collectionView registerClass:[ActuationWorklogSandboxHostCell class]
      forCellWithReuseIdentifier:kWorklogCellIdentifier];
  [_collectionView registerClass:[UICollectionViewCell class]
      forCellWithReuseIdentifier:kResponseCellIdentifier];
  [self.view addSubview:_collectionView];

  _worklogView = [[ActuationWorklogView alloc] initWithFrame:CGRectZero];
  _worklogView.translatesAutoresizingMaskIntoConstraints = NO;

  UIView* panelContainer = [[UIView alloc] init];
  panelContainer.translatesAutoresizingMaskIntoConstraints = NO;
  panelContainer.backgroundColor =
      [UIColor colorNamed:kSecondaryBackgroundColor];
  [self.view addSubview:panelContainer];

  UIView* separator = [self createSeparatorView];
  [panelContainer addSubview:separator];

  UIStackView* inputStack = [[UIStackView alloc] init];
  inputStack.axis = UILayoutConstraintAxisVertical;
  inputStack.spacing = kSpacingSmall;
  inputStack.translatesAutoresizingMaskIntoConstraints = NO;
  [panelContainer addSubview:inputStack];

  UILabel* stepHeader = [self headerLabel:@"STEP CONFIGURATION"];
  [inputStack addArrangedSubview:stepHeader];

  UIStackView* stepContainer = [[UIStackView alloc] init];
  stepContainer.spacing = kSpacingMedium;
  stepContainer.alignment = UIStackViewAlignmentTop;
  [inputStack addArrangedSubview:stepContainer];

  UIStackView* stepInputsStack = [[UIStackView alloc] init];
  stepInputsStack.axis = UILayoutConstraintAxisVertical;
  stepInputsStack.spacing = kSpacingSmall;
  [stepContainer addArrangedSubview:stepInputsStack];

  UIStackView* stepTitleRow = [self rowWithSpacing:kSpacingSmall];
  [stepInputsStack addArrangedSubview:stepTitleRow];

  _titleField = [self textField:@"Step Title"];
  [stepTitleRow addArrangedSubview:_titleField];

  _actuatingSwitch = [[UISwitch alloc] init];
  _actuatingSwitch.on = YES;
  [_actuatingSwitch addTarget:self
                       action:@selector(actuatingSwitchChanged:)
             forControlEvents:UIControlEventValueChanged];
  [stepTitleRow addArrangedSubview:[self labelWithText:@"Actuating"]];
  [stepTitleRow addArrangedSubview:_actuatingSwitch];

  _subtitleField = [self textField:@"Step Subtitle (optional)"];
  [stepInputsStack addArrangedSubview:_subtitleField];

  _stepStyleSelector =
      [self segmentedControl:@[ @"Dot", @"Label", @"Card", @"Text" ]];
  [stepInputsStack addArrangedSubview:_stepStyleSelector];

  UIStackView* stepButtonsStack =
      [[UIStackView alloc] initWithArrangedSubviews:@[
        [self circleButton:SymbolPlus
                      tint:[UIColor colorNamed:kBlueColor]
                    action:@selector(add:)],
        [self circleButton:SymbolArrowClockWise
                      tint:[UIColor colorNamed:kGreenColor]
                    action:@selector(update:)],
        [self circleButton:SymbolTrash
                      tint:[UIColor colorNamed:kOrange500Color]
                    action:@selector(pop:)],
      ]];
  stepButtonsStack.axis = UILayoutConstraintAxisVertical;
  stepButtonsStack.spacing = kSpacingSmall;
  stepButtonsStack.distribution = UIStackViewDistributionFillEqually;
  [stepContainer addArrangedSubview:stepButtonsStack];

  UIView* sectionSeparator = [self createSeparatorView];
  [inputStack addArrangedSubview:sectionSeparator];

  UILabel* chipHeader = [self headerLabel:@"CHIP CONFIGURATION"];
  [inputStack addArrangedSubview:chipHeader];

  UIStackView* chipContainer = [[UIStackView alloc] init];
  chipContainer.spacing = kSpacingMedium;
  chipContainer.alignment = UIStackViewAlignmentTop;
  [inputStack addArrangedSubview:chipContainer];

  UIStackView* chipInputsStack = [[UIStackView alloc] init];
  chipInputsStack.axis = UILayoutConstraintAxisVertical;
  chipInputsStack.spacing = kSpacingSmall;
  [chipContainer addArrangedSubview:chipInputsStack];

  _chipField = [self textField:@"Active Chip Text"];
  [chipInputsStack addArrangedSubview:_chipField];

  UIStackView* chipIconRow = [self rowWithSpacing:kSpacingSmall];
  [chipInputsStack addArrangedSubview:chipIconRow];

  _chipIconSelector = [self segmentedControl:@[ @"None", @"Search", @"Globe" ]];
  [chipIconRow addArrangedSubview:_chipIconSelector];

  UIView* chipSpacer = [[UIView alloc] init];
  [chipSpacer setContentHuggingPriority:UILayoutPriorityDefaultLow
                                forAxis:UILayoutConstraintAxisHorizontal];
  [chipIconRow addArrangedSubview:chipSpacer];

  UIButton* setChipButton = [self circleButton:SymbolPencil
                                          tint:[UIColor colorNamed:kBlueColor]
                                        action:@selector(setChip:)];
  [chipContainer addArrangedSubview:setChipButton];

  UIView* resetSeparator = [self createSeparatorView];
  [inputStack addArrangedSubview:resetSeparator];

  UIButtonConfiguration* resetConfig =
      [UIButtonConfiguration plainButtonConfiguration];
  resetConfig.title = @"Reset Sandbox";
  resetConfig.baseForegroundColor = [UIColor colorNamed:kRedColor];
  resetConfig.buttonSize = UIButtonConfigurationSizeSmall;

  UIButton* resetButton = [UIButton buttonWithType:UIButtonTypeSystem];
  resetButton.configuration = resetConfig;
  [resetButton addTarget:self
                  action:@selector(reset:)
        forControlEvents:UIControlEventTouchUpInside];
  [inputStack addArrangedSubview:resetButton];

  [NSLayoutConstraint activateConstraints:@[
    [_collectionView.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor],
    [_collectionView.bottomAnchor
        constraintEqualToAnchor:panelContainer.topAnchor],
    [panelContainer.bottomAnchor
        constraintEqualToAnchor:self.view.keyboardLayoutGuide.topAnchor],
    [inputStack.bottomAnchor
        constraintEqualToAnchor:panelContainer.safeAreaLayoutGuide.bottomAnchor
                       constant:-kSpacingSmall],
    [stepButtonsStack.widthAnchor constraintEqualToConstant:kSpacingLarge * 2],
    [setChipButton.widthAnchor constraintEqualToConstant:kSpacingLarge * 2],
  ]];

  AddSameConstraintsToSidesWithInsets(
      _collectionView, self.view,
      LayoutSides::kLeading | LayoutSides::kTrailing,
      NSDirectionalEdgeInsetsMake(0, kSpacingMedium, 0, kSpacingMedium));
  AddSameConstraintsToSides(panelContainer, self.view,
                            LayoutSides::kLeading | LayoutSides::kTrailing);
  AddSameConstraintsToSides(
      separator, panelContainer,
      LayoutSides::kTop | LayoutSides::kLeading | LayoutSides::kTrailing);
  AddSameConstraintsToSidesWithInsets(
      inputStack, panelContainer,
      LayoutSides::kTop | LayoutSides::kLeading | LayoutSides::kTrailing,
      NSDirectionalEdgeInsetsMake(kSpacingLarge, kSpacingLarge, 0,
                                  kSpacingLarge));

  [self populateMockWorklog];
}

- (void)populateMockWorklog {
  ActuationWorklogItem* step1 = [[ActuationWorklogItem alloc]
      initWithTitle:@"Task started"
           subtitle:@"Use Gemini carefully and take control if needed. You are "
                    @"responsible for Gemini's actions during tasks."
               icon:SymbolTemplateWithPointSize(SymbolPlayFill, kIconSize)
              style:ActuationWorklogItemStyle::kLabeled];
  ActuationWorklogItem* step2 = [self item:@"Opening a new Tab."];
  ActuationWorklogItem* step3 = [self item:@"Searching theaters."];
  ActuationWorklogItem* step4 = [self item:@"Searching showtimes."];
  ActuationWorklogItem* step5 = [self item:@"Finding MTL venues."];

  _worklogItems = [@[ step1, step2, step3, step4, step5 ] mutableCopy];
  [_worklogView setItems:_worklogItems];
}

#pragma mark - Actions

- (void)add:(id)sender {
  ActuationWorklogItem* newItem = [self stepFromInputs:@"New Step"];
  [_worklogItems addObject:newItem];
  [_worklogView addItem:newItem];
  [self resetStepConfigSection];
  [self scrollToBottomAnimated:YES];
}

- (void)pop:(id)sender {
  if (_worklogItems.count == 0) {
    return;
  }
  [_worklogItems removeLastObject];
  [_worklogView setLastItem:nil];
  [self scrollToBottomAnimated:YES];
}

- (void)update:(id)sender {
  if (_worklogItems.count == 0) {
    return;
  }

  ActuationWorklogItem* updatedItem = [self stepFromInputs:@"Updated Step"];
  _worklogItems[_worklogItems.count - 1] = updatedItem;
  [_worklogView setLastItem:updatedItem];
  [self resetStepConfigSection];
  [self scrollToBottomAnimated:YES];
}

- (void)setChip:(id)sender {
  NSString* chipText = _chipField.text;
  if (chipText.length == 0) {
    [_worklogView setChip:nil];
    return;
  }

  UIImage* icon = [self currentChipIcon];
  ActuationWorklogChip* chip =
      [[ActuationWorklogChip alloc] initWithText:chipText icon:icon];
  [_worklogView setChip:chip];
  [self scrollToBottomAnimated:YES];
}

- (void)reset:(id)sender {
  [self populateMockWorklog];
  [self resetStepConfigSection];
  [self resetChipSection];
  _actuatingSwitch.on = YES;
  _worklogView.actuating = YES;
  [self scrollToBottomAnimated:YES];
}

- (void)actuatingSwitchChanged:(UISwitch*)sender {
  _worklogView.actuating = sender.on;
}

#pragma mark - Helpers

// Coalesces height-change callbacks from `ActuationWorklogSandboxHostCell` onto
// the next main-queue turn, matching the Gemini SDK's
// `remeasureCustomContentItemWithBlockID:`.
- (void)scheduleWorklogCellResize {
  if (_worklogResizeScheduled) {
    return;
  }
  _worklogResizeScheduled = YES;
  __weak __typeof(self) weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    [weakSelf performWorklogCellResize];
  });
}

// Reconfigures the hosted worklog cell inside an animated batch update so
// sibling response blocks animate their Y-position changes, then performs any
// scroll that was waiting for the new cell height.
- (void)performWorklogCellResize {
  _worklogResizeScheduled = NO;
  [_collectionView
      performBatchUpdates:^{
        [_collectionView
            reconfigureItemsAtIndexPaths:@[ [NSIndexPath indexPathForItem:0
                                                                inSection:0] ]];
      }
               completion:nil];
  if (std::optional<BOOL> animated =
          std::exchange(_pendingScrollToBottomAnimated, std::nullopt)) {
    [self applyScrollToBottomAnimated:*animated];
  }
}

// Scrolls to the bottom of the collection view. Laying out first lets a worklog
// height change schedule its cell resize; the scroll then waits for that resize
// so it targets the new content size.
- (void)scrollToBottomAnimated:(BOOL)animated {
  [self.view layoutIfNeeded];
  if (_worklogResizeScheduled) {
    _pendingScrollToBottomAnimated = animated;
    return;
  }
  [self applyScrollToBottomAnimated:animated];
}

// Scrolls to the bottom of the current content size.
- (void)applyScrollToBottomAnimated:(BOOL)animated {
  [self.view layoutIfNeeded];
  [_collectionView layoutIfNeeded];
  CGPoint bottomOffset =
      CGPointMake(0, _collectionView.contentSize.height -
                         _collectionView.bounds.size.height +
                         _collectionView.adjustedContentInset.bottom);
  if (bottomOffset.y > 0) {
    [_collectionView setContentOffset:bottomOffset animated:animated];
  }
}

- (ActuationWorklogItem*)item:(NSString*)title {
  return [ActuationWorklogItem simpleItemWithTitle:title];
}

- (ActuationWorklogItem*)stepFromInputs:(NSString*)defaultTitle {
  NSString* title = _titleField.text;
  if (title.length == 0) {
    title = defaultTitle;
  }
  NSString* subtitle = _subtitleField.text;
  UIImage* icon = SymbolTemplateWithPointSize(SymbolPlayFill, kIconSize);
  ActuationWorklogItemStyle style = [self currentItemStyle];
  if (style == ActuationWorklogItemStyle::kFreeform) {
    return [ActuationWorklogItem freeformItemWithText:title];
  }

  return [[ActuationWorklogItem alloc] initWithTitle:title
                                            subtitle:subtitle
                                                icon:icon
                                               style:style];
}

- (UILabel*)headerLabel:(NSString*)text {
  UILabel* label = [[UILabel alloc] init];
  label.text = text;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleCaption2];
  label.textColor = [UIColor colorNamed:kTextSecondaryColor];
  return label;
}

- (UIView*)createSeparatorView {
  UIView* separator = [[UIView alloc] init];
  separator.backgroundColor = [UIColor colorNamed:kSeparatorColor];
  separator.translatesAutoresizingMaskIntoConstraints = NO;
  [separator.heightAnchor constraintEqualToConstant:1.0].active = YES;
  return separator;
}

- (UILabel*)labelWithText:(NSString*)text {
  UILabel* label = [[UILabel alloc] init];
  label.text = text;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  label.textColor = [UIColor colorNamed:kTextSecondaryColor];
  return label;
}

- (UITextField*)textField:(NSString*)placeholder {
  UITextField* textField = [[UITextField alloc] init];
  textField.placeholder = placeholder;
  textField.borderStyle = UITextBorderStyleRoundedRect;
  textField.delegate = self;
  return textField;
}

- (UIStackView*)rowWithSpacing:(CGFloat)spacing {
  UIStackView* row = [[UIStackView alloc] init];
  row.spacing = spacing;
  row.alignment = UIStackViewAlignmentCenter;
  return row;
}

- (UISegmentedControl*)segmentedControl:(NSArray<NSString*>*)items {
  UISegmentedControl* view = [[UISegmentedControl alloc] initWithItems:items];
  view.selectedSegmentIndex = 0;
  return view;
}

- (UIButton*)circleButton:(Symbol)symbol
                     tint:(UIColor*)tintColor
                   action:(SEL)action {
  UIButtonConfiguration* config =
      [UIButtonConfiguration tintedButtonConfiguration];
  config.image = SymbolTemplateWithPointSize(symbol, kIconSize);
  config.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
  config.baseForegroundColor = tintColor;
  config.baseBackgroundColor = [tintColor colorWithAlphaComponent:0.45];

  UIButton* button = [UIButton buttonWithConfiguration:config
                                         primaryAction:nil];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  [button.heightAnchor constraintEqualToAnchor:button.widthAnchor].active = YES;
  [button addTarget:self
                action:action
      forControlEvents:UIControlEventTouchUpInside];
  return button;
}

- (void)resetStepConfigSection {
  _titleField.text = @"";
  _subtitleField.text = @"";
  _stepStyleSelector.selectedSegmentIndex = 0;
  [_titleField resignFirstResponder];
  [_subtitleField resignFirstResponder];
}

- (void)resetChipSection {
  _chipField.text = @"";
  _chipIconSelector.selectedSegmentIndex = 0;
  [_chipField resignFirstResponder];
  [_worklogView setChip:nil];
}

- (UIImage*)currentChipIcon {
  switch (_chipIconSelector.selectedSegmentIndex) {
    case 0:
      return nil;
    case 1:
      return SymbolTemplateWithPointSize(SymbolMagnifyingglass, kIconSize);
    case 2:
      return SymbolTemplateWithPointSize(SymbolGlobe, kIconSize);
    default:
      NOTREACHED();
  }
}

- (ActuationWorklogItemStyle)currentItemStyle {
  switch (_stepStyleSelector.selectedSegmentIndex) {
    case 0:
      return ActuationWorklogItemStyle::kSimple;
    case 1:
      return ActuationWorklogItemStyle::kLabeled;
    case 2:
      return ActuationWorklogItemStyle::kCard;
    case 3:
      return ActuationWorklogItemStyle::kFreeform;
    default:
      NOTREACHED();
  }
}

#pragma mark - UICollectionViewDataSource

- (NSInteger)collectionView:(UICollectionView*)collectionView
     numberOfItemsInSection:(NSInteger)section {
  return kSandboxCellCount;
}

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  if (indexPath.item == 0) {
    ActuationWorklogSandboxHostCell* cell =
        base::apple::ObjCCastStrict<ActuationWorklogSandboxHostCell>(
            [collectionView
                dequeueReusableCellWithReuseIdentifier:kWorklogCellIdentifier
                                          forIndexPath:indexPath]);
    cell.backgroundConfiguration =
        [UIBackgroundConfiguration clearConfiguration];
    __weak __typeof(self) weakSelf = self;
    cell.hostedViewHeightDidChange = ^{
      [weakSelf scheduleWorklogCellResize];
    };
    [cell hostView:_worklogView];
    return cell;
  }

  UICollectionViewCell* cell = [collectionView
      dequeueReusableCellWithReuseIdentifier:kResponseCellIdentifier
                                forIndexPath:indexPath];
  UIListContentConfiguration* content =
      [UIListContentConfiguration cellConfiguration];
  content.text = kMockResponseSummary;
  content.textProperties.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  content.textProperties.color = [UIColor colorNamed:kTextSecondaryColor];
  cell.contentConfiguration = content;

  UIBackgroundConfiguration* background =
      [UIBackgroundConfiguration clearConfiguration];
  background.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
  background.cornerRadius = kSpacingMedium;
  cell.backgroundConfiguration = background;
  return cell;
}

#pragma mark - UITextFieldDelegate

- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  [textField resignFirstResponder];
  return YES;
}

@end
