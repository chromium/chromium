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
constexpr CGFloat kMargin = 8;

// The padding used inside the list.
constexpr CGFloat kPadding = 15;

// The point size used for the menu button and expand button.
constexpr CGFloat kButtonPointSize = 17;

// The duration of the animation used when expanding and unexpanding the list.
constexpr base::TimeDelta kExpandAnimationDuration = base::Seconds(0.25);

// The accessibility IDs used for various UI items.
constexpr NSString* const kSetUpListAccessibilityID =
    @"kSetUpListAccessibilityID";
constexpr NSString* const kSetUpListExpandButtonID =
    @"kSetUpListExpandButtonID";
constexpr NSString* const kSetUpListMenuButtonID = @"kSetUpListMenuButtonID";

}  //  namespace

@interface SetUpListView () <SetUpListItemViewTapDelegate>
@end

@implementation SetUpListView {
  // The array of item data given to the initializer.
  NSArray<SetUpListItemViewData*>* _itemsData;

  // The array of SetUpListItemViews.
  NSMutableArray<SetUpListItemView*>* _items;

  // The stack view that holds all the items.
  UIStackView* _itemsStack;

  // The button that expands and unexpands the items list.
  UIButton* _expandButton;

  // Whether the item list is expanded to show all items.
  BOOL _expanded;
}

- (instancetype)initWithItems:(NSArray<SetUpListItemViewData*>*)items {
  self = [super init];
  if (self) {
    CHECK_GT(items.count, 0ul);
    _itemsData = items;
  }
  return self;
}

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  [super willMoveToSuperview:newSuperview];

  [self createSubviews];
}

#pragma mark - SetUpListItemViewTapDelegate methods

- (void)didTapSetUpListItemView:(SetUpListItemView*)view {
  [self.delegate didSelectSetUpListItem:view.type];
}

#pragma mark - Private methods (UI events)

// Calls the command handler to notify that the menu button was tapped.
- (void)didTapMenuButton {
  [self.delegate showSetUpListMenu];
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
  if (_items.count > kInitialItemCount) {
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

  UIButton* menuButton = [self createMenuButton];
  [self addSubview:menuButton];
  [NSLayoutConstraint activateConstraints:@[
    [menuButton.trailingAnchor
        constraintEqualToAnchor:containerStack.trailingAnchor],
    [menuButton.firstBaselineAnchor
        constraintEqualToAnchor:listTitle.firstBaselineAnchor],
  ]];

  if (_expandButton) {
    self.accessibilityElements =
        @[ listTitle, menuButton, _itemsStack, _expandButton ];
  } else {
    self.accessibilityElements = @[ listTitle, menuButton, _itemsStack ];
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
  UIStackView* stack =
      [[UIStackView alloc] initWithArrangedSubviews:[self initialItems]];
  stack.axis = UILayoutConstraintAxisVertical;
  stack.translatesAutoresizingMaskIntoConstraints = NO;
  stack.spacing = kPadding;
  stack.layer.masksToBounds = YES;
  stack.layer.cornerRadius = kBorderRadius;
  stack.layer.borderWidth = kBorderWidth;
  stack.layer.borderColor = [UIColor colorNamed:kGrey200Color].CGColor;
  stack.layoutMarginsRelativeArrangement = YES;
  stack.layoutMargins =
      UIEdgeInsetsMake(kPadding, kPadding, kPadding, kPadding);

  stack.accessibilityLabel = l10n_util::GetNSString(IDS_IOS_SET_UP_LIST_TITLE);
  stack.accessibilityContainerType = UIAccessibilityContainerTypeList;
  stack.accessibilityElements = [self initialItems];
  ;
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
                   }];
}

// Unexpands the items stack view, to only show a limited number again.
- (void)unexpandItemsStack {
  _expanded = NO;
  NSArray<SetUpListItemView*>* items = [self expandedItems];
  _itemsStack.accessibilityElements = [self initialItems];
  _expandButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_SET_UP_LIST_EXPAND);

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
      }
      completion:^(BOOL finished) {
        for (SetUpListItemView* item in items) {
          [item removeFromSuperview];
        }
      }];
}

@end
