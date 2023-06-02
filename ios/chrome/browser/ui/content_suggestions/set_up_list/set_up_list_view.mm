// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_view.h"

#import "base/time/time.h"
#import "ios/chrome/browser/ntp/set_up_list_item_type.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_item_view.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_item_view_data.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/dynamic_type_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The number of items to display initially, until the user expands the list.
constexpr int kInitialItemCount = 2;

// The width of the border around the list.
constexpr CGFloat kBorderWidth = 1;

// The radius of the rounded corner used for the border around the list.
constexpr CGFloat kBorderRadius = 12;

// The margin from the leading/trailing sides to the list.
constexpr CGFloat kMargin = 14;

// The padding used inside the list.
constexpr CGFloat kPadding = 15;

// The spacing used between title and description in the "All Set" View.
constexpr CGFloat kAllSetSpacing = 10;

// The point size used for the menu button and expand button.
constexpr CGFloat kButtonPointSize = 17;

// The duration of the animation used when expanding and unexpanding the list.
constexpr base::TimeDelta kExpandAnimationDuration = base::Seconds(0.25);

// The duration of the animation used when displaying the "All Set" view.
constexpr base::TimeDelta kAllSetAnimationDuration = base::Seconds(0.5);

// The names of images used on the left and right sides of the "All Set" view.
constexpr NSString* const kAllSetLeft = @"set_up_list_all_set_left";
constexpr NSString* const kAllSetRight = @"set_up_list_all_set_right";

// The accessibility IDs used for various UI items.
constexpr NSString* const kSetUpListAccessibilityID =
    @"kSetUpListAccessibilityID";
constexpr NSString* const kSetUpListExpandButtonID =
    @"kSetUpListExpandButtonID";
constexpr NSString* const kSetUpListMenuButtonID = @"kSetUpListMenuButtonID";
constexpr NSString* const kSetUpListAllSetID = @"kSetUpListAllSetID";

}  //  namespace

@interface SetUpListView () <SetUpListItemViewTapDelegate>
@end

@implementation SetUpListView {
  // The array of item data given to the initializer.
  NSArray<SetUpListItemViewData*>* _itemsData;

  // The view that needs layout if SetUpListView's height changes.
  UIView* _rootView;

  // The array of SetUpListItemViews.
  NSMutableArray<SetUpListItemView*>* _items;

  // The stack view that holds all the items.
  UIStackView* _itemsStack;

  // The button that expands and unexpands the items list.
  UIButton* _expandButton;

  // Whether the item list is expanded to show all items.
  BOOL _expanded;

  // The button that opens the Set Up List menu.
  UIButton* _menuButton;
}

- (instancetype)initWithItems:(NSArray<SetUpListItemViewData*>*)items
                     rootView:(UIView*)rootView {
  self = [super init];
  if (self) {
    CHECK_GT(items.count, 0ul);
    _itemsData = items;
    _rootView = rootView;
  }
  return self;
}

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  [super willMoveToSuperview:newSuperview];

  [self createSubviews];
}

#pragma mark - UITraitEnvironment

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (previousTraitCollection.userInterfaceStyle !=
      self.traitCollection.userInterfaceStyle) {
    _itemsStack.layer.borderColor =
        [UIColor colorNamed:kSeparatorColor].CGColor;
  }
}

#pragma mark - Public

- (void)markItemComplete:(SetUpListItemType)type
              completion:(ProceduralBlock)completion {
  for (SetUpListItemView* item in _items) {
    if (item.type == type) {
      [item markCompleteWithCompletion:completion];
      break;
    }
  }
}

- (void)showDoneWithAnimations:(ProceduralBlock)animations {
  UIView* allSetView = [self createAllSetView];
  allSetView.alpha = 0;
  allSetView.hidden = YES;
  [_itemsStack addSubview:allSetView];
  // Position the allSetView the same way the _itemsStack will, so that it
  // doesn't move during the animation. It should only fade in.
  [NSLayoutConstraint activateConstraints:@[
    [allSetView.leadingAnchor
        constraintEqualToAnchor:_itemsStack.layoutMarginsGuide.leadingAnchor],
    [allSetView.trailingAnchor
        constraintEqualToAnchor:_itemsStack.layoutMarginsGuide.trailingAnchor],
    [allSetView.topAnchor
        constraintEqualToAnchor:_itemsStack.layoutMarginsGuide.topAnchor],
  ]];

  [_itemsStack layoutIfNeeded];
  _itemsStack.accessibilityElements = @[ allSetView ];
  __weak __typeof(_itemsStack) weakItemsStack = _itemsStack;
  [UIView animateWithDuration:kAllSetAnimationDuration.InSecondsF()
      animations:^{
        for (UIView* view in weakItemsStack.arrangedSubviews) {
          view.alpha = 0;
          view.hidden = YES;
          // Constrain the old item view's position so that it doesn't move
          // during the animation. It should only fade out.
          [NSLayoutConstraint activateConstraints:@[
            [view.heightAnchor
                constraintEqualToConstant:view.frame.size.height],
            [view.widthAnchor constraintEqualToConstant:view.frame.size.width],
            [view.topAnchor constraintEqualToAnchor:weakItemsStack.topAnchor
                                           constant:view.frame.origin.y],
            [view.leftAnchor constraintEqualToAnchor:weakItemsStack.leftAnchor
                                            constant:view.frame.origin.x],
          ]];
          [weakItemsStack removeArrangedSubview:view];
        }
        [weakItemsStack insertArrangedSubview:allSetView atIndex:0];
        allSetView.alpha = 1;
        allSetView.hidden = NO;
        if (animations) {
          animations();
        }
      }
      completion:^(BOOL finished) {
        for (UIView* view in weakItemsStack.arrangedSubviews) {
          if (view != allSetView) {
            [view removeFromSuperview];
          }
        }
      }];
}

#pragma mark - SetUpListItemViewTapDelegate methods

- (void)didTapSetUpListItemView:(SetUpListItemView*)view {
  [self.delegate didSelectSetUpListItem:view.type];
}

#pragma mark - Private methods (UI events)

// Calls the command handler to notify that the menu button was tapped.
- (void)didTapMenuButton {
  [self.delegate showSetUpListMenuWithButton:_menuButton];
}

// Toggles the expanded state of the items stack view.
- (void)didTapExpandButton {
  if (_expanded) {
    [self unexpandItemsStack];
  } else {
    [self expandItemsStack];
  }
}

#pragma mark - Private methods (helpers to create subviews)

// Creates all the subviews for this view.
- (void)createSubviews {
  // Return if the subviews have already been created and added.
  if (!(self.subviews.count == 0)) {
    return;
  }

  self.translatesAutoresizingMaskIntoConstraints = NO;
  self.accessibilityIdentifier = kSetUpListAccessibilityID;

  UILabel* listTitle = [self createListTitle];
  _items = [self createItems];
  _itemsStack = [self createItemsStack];
  if (_items.count > kInitialItemCount && ![self allItemsComplete]) {
    _expandButton = [self createExpandButton];
    [_itemsStack addArrangedSubview:_expandButton];
  }

  UIStackView* containerStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ listTitle, _itemsStack ]];
  containerStack.translatesAutoresizingMaskIntoConstraints = NO;
  containerStack.axis = UILayoutConstraintAxisVertical;
  containerStack.spacing = kPadding;
  [self addSubview:containerStack];
  AddSameConstraintsWithInsets(
      containerStack, self,
      NSDirectionalEdgeInsetsMake(0, kMargin, 0, kMargin));

  _menuButton = [self createMenuButton];
  [self addSubview:_menuButton];
  [NSLayoutConstraint activateConstraints:@[
    [_menuButton.trailingAnchor
        constraintEqualToAnchor:containerStack.trailingAnchor],
    [_menuButton.firstBaselineAnchor
        constraintEqualToAnchor:listTitle.firstBaselineAnchor],
  ]];

  if (_expandButton) {
    self.accessibilityElements =
        @[ listTitle, _menuButton, _itemsStack, _expandButton ];
  } else {
    self.accessibilityElements = @[ listTitle, _menuButton, _itemsStack ];
  }
}

// Returns an array of SetUpListItemViews based on the itemsDictionary that was
// given to the initializer.
- (NSMutableArray<SetUpListItemView*>*)createItems {
  NSMutableArray<SetUpListItemView*>* items = [[NSMutableArray alloc] init];
  NSMutableArray<SetUpListItemView*>* completed = [[NSMutableArray alloc] init];

  for (SetUpListItemViewData* data in _itemsData) {
    SetUpListItemView* view = [[SetUpListItemView alloc] initWithData:data];
    view.tapDelegate = self;
    if (data.complete) {
      [completed addObject:view];
    } else {
      [items addObject:view];
    }
  }
  // Completed items appear at the end of the list.
  [items addObjectsFromArray:completed];
  return items;
}

// Creates the vertical stack view that holds all the SetUpListItemViews.
- (UIStackView*)createItemsStack {
  _expanded = NO;
  BOOL allItemsComplete = [self allItemsComplete];
  NSArray<UIView*>* initialItems;
  if (allItemsComplete) {
    initialItems = @[ [self createAllSetView] ];
  } else {
    initialItems = [self initialItems];
  }
  UIStackView* stack =
      [[UIStackView alloc] initWithArrangedSubviews:initialItems];
  stack.axis = UILayoutConstraintAxisVertical;
  stack.translatesAutoresizingMaskIntoConstraints = NO;
  stack.spacing = kPadding;
  stack.layer.masksToBounds = YES;
  stack.layer.cornerRadius = kBorderRadius;
  stack.layer.borderWidth = kBorderWidth;
  stack.layer.borderColor = [UIColor colorNamed:kSeparatorColor].CGColor;
  stack.layoutMarginsRelativeArrangement = YES;
  stack.layoutMargins =
      UIEdgeInsetsMake(kPadding, kPadding, kPadding, kPadding);

  stack.accessibilityLabel = l10n_util::GetNSString(IDS_IOS_SET_UP_LIST_TITLE);
  stack.accessibilityContainerType = UIAccessibilityContainerTypeList;
  stack.accessibilityElements = [self initialItems];
  return stack;
}

// Creates the title label at the top of the Set Up List.
- (UILabel*)createListTitle {
  UILabel* label = [[UILabel alloc] init];
  label.text = l10n_util::GetNSString(IDS_IOS_SET_UP_LIST_TITLE);
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  label.textColor = [UIColor colorNamed:kTextSecondaryColor];
  label.accessibilityTraits = UIAccessibilityTraitHeader;
  return label;
}

// Creates the menu button at the top of the Set Up List.
- (UIButton*)createMenuButton {
  UIButton* button = [[ExtendedTouchTargetButton alloc] init];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  UIImage* icon =
      DefaultSymbolTemplateWithPointSize(kMenuSymbol, kButtonPointSize);
  [button setImage:icon forState:UIControlStateNormal];
  button.tintColor = [UIColor colorNamed:kGrey600Color];

  button.accessibilityIdentifier = kSetUpListMenuButtonID;
  button.isAccessibilityElement = YES;
  button.accessibilityLabel = l10n_util::GetNSString(IDS_IOS_SET_UP_LIST_MENU);

  [button addTarget:self
                action:@selector(didTapMenuButton)
      forControlEvents:UIControlEventTouchUpInside];
  return button;
}

// Creates the button at the bottom of the list that allows the user to toggle
// the expanded state of the list.
- (UIButton*)createExpandButton {
  UIButton* button = [[ExtendedTouchTargetButton alloc] init];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  UIImage* icon =
      DefaultSymbolTemplateWithPointSize(kChevronDownSymbol, kButtonPointSize);
  [button setImage:icon forState:UIControlStateNormal];
  button.tintColor = [UIColor colorNamed:kGrey600Color];

  button.accessibilityIdentifier = kSetUpListExpandButtonID;
  button.isAccessibilityElement = YES;
  button.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_SET_UP_LIST_EXPAND);

  [button addTarget:self
                action:@selector(didTapExpandButton)
      forControlEvents:UIControlEventTouchUpInside];
  return button;
}

// Creates a view with a message that indicates that all the items have been
// completed.
- (UIView*)createAllSetView {
  UILabel* title = [[UILabel alloc] init];
  title.text = l10n_util::GetNSString(IDS_IOS_SET_UP_LIST_ALL_SET_TITLE);
  title.textAlignment = NSTextAlignmentCenter;
  title.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  title.textColor = [UIColor colorNamed:kTextPrimaryColor];
  title.accessibilityTraits = UIAccessibilityTraitHeader;

  UILabel* description = [[UILabel alloc] init];
  description.text =
      l10n_util::GetNSString(IDS_IOS_SET_UP_LIST_ALL_SET_DESCRIPTION);
  description.textAlignment = NSTextAlignmentCenter;
  description.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  description.textColor = [UIColor colorNamed:kTextSecondaryColor];
  description.numberOfLines = 0;
  description.lineBreakMode = NSLineBreakByWordWrapping;

  UIStackView* stack =
      [[UIStackView alloc] initWithArrangedSubviews:@[ title, description ]];
  stack.accessibilityIdentifier = kSetUpListAllSetID;
  stack.axis = UILayoutConstraintAxisVertical;
  stack.alignment = UIStackViewAlignmentCenter;
  stack.translatesAutoresizingMaskIntoConstraints = NO;
  stack.spacing = kAllSetSpacing;
  stack.layoutMarginsRelativeArrangement = YES;
  stack.layoutMargins =
      UIEdgeInsetsMake(kPadding, kPadding, kPadding, kPadding);

  UIImageView* leftImage =
      [[UIImageView alloc] initWithImage:[UIImage imageNamed:kAllSetLeft]];
  UIImageView* rightImage =
      [[UIImageView alloc] initWithImage:[UIImage imageNamed:kAllSetRight]];
  leftImage.translatesAutoresizingMaskIntoConstraints = NO;
  rightImage.translatesAutoresizingMaskIntoConstraints = NO;
  [stack addSubview:leftImage];
  [stack addSubview:rightImage];
  AddSameCenterYConstraint(leftImage, stack);
  AddSameCenterYConstraint(rightImage, stack);
  [NSLayoutConstraint activateConstraints:@[
    [leftImage.leftAnchor constraintEqualToAnchor:stack.leftAnchor],
    [rightImage.rightAnchor constraintEqualToAnchor:stack.rightAnchor],
  ]];
  return stack;
}

#pragma mark - Private methods (expandable stack helpers)

// Returns the initial items for display (i.e. at most the first two).
- (NSArray<SetUpListItemView*>*)initialItems {
  if (_items.count <= kInitialItemCount) {
    return _items;
  }

  NSRange range = NSMakeRange(0, kInitialItemCount);
  return [_items subarrayWithRange:range];
}

// Returns the items that should be added to expand the list (or removed to
// unexpand).
- (NSArray<SetUpListItemView*>*)expandedItems {
  if (_items.count <= kInitialItemCount) {
    return @[];
  }

  NSRange range =
      NSMakeRange(kInitialItemCount, _items.count - kInitialItemCount);
  return [_items subarrayWithRange:range];
}

// Expands the items stack view to show all items.
- (void)expandItemsStack {
  _expanded = YES;
  NSArray<SetUpListItemView*>* items = [self expandedItems];
  _expandButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_SET_UP_LIST_COLLAPSE);

  int index = 2;
  for (SetUpListItemView* item in items) {
    item.alpha = 0;
    item.hidden = YES;
    [_itemsStack insertArrangedSubview:item atIndex:index];
    index++;
  }
  _itemsStack.accessibilityElements = _items;
  // Layout the newly added (but still hidden) items, so that the animation is
  // correct.
  [self setNeedsLayout];
  [self layoutIfNeeded];

  __weak __typeof(self) weakSelf = self;
  __weak __typeof(_expandButton) weakExpandButton = _expandButton;
  [UIView animateWithDuration:kExpandAnimationDuration.InSecondsF()
                   animations:^{
                     for (SetUpListItemView* item in items) {
                       item.alpha = 1;
                       item.hidden = NO;
                     }
                     // Flip the expand button to point up;
                     weakExpandButton.transform = CGAffineTransformScale(
                         CGAffineTransformIdentity, 1, -1);
                     [weakSelf heightDidChange];
                   }];
}

// Unexpands the items stack view, to only show a limited number again.
- (void)unexpandItemsStack {
  _expanded = NO;
  NSArray<SetUpListItemView*>* items = [self expandedItems];
  _itemsStack.accessibilityElements = [self initialItems];
  _expandButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_SET_UP_LIST_EXPAND);

  __weak __typeof(self) weakSelf = self;
  __weak __typeof(_expandButton) weakExpandButton = _expandButton;
  [UIView animateWithDuration:kExpandAnimationDuration.InSecondsF()
      animations:^{
        for (SetUpListItemView* item in items) {
          item.alpha = 0;
          item.hidden = YES;
        }
        // Flip the expand button to point down again;
        weakExpandButton.transform =
            CGAffineTransformScale(CGAffineTransformIdentity, 1, 1);
        [weakSelf heightDidChange];
      }
      completion:^(BOOL finished) {
        for (SetUpListItemView* item in items) {
          [item removeFromSuperview];
        }
      }];
}

// Tells the root view to re-layout and tells the delegate that the height
// changed.
- (void)heightDidChange {
  [_rootView setNeedsLayout];
  [_rootView layoutIfNeeded];
  [self.delegate setUpListViewHeightDidChange];
}

#pragma mark Private methods (All Set view)

// Returns `YES` if all items are marked complete.
- (BOOL)allItemsComplete {
  for (SetUpListItemView* item in _items) {
    if (!item.complete) {
      return NO;
    }
  }
  return YES;
}

@end
