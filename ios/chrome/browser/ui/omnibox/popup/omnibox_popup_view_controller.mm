// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_view_controller.h"

#include <memory>

#include "base/ios/ios_util.h"
#import "ios/chrome/browser/ui/omnibox/image_retriever.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_util.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_row.h"
#import "ios/chrome/browser/ui/omnibox/popup/self_sizing_table_view.h"
#import "ios/chrome/browser/ui/omnibox/truncating_attributed_label.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#include "ios/chrome/browser/ui/util/animation_util.h"
#include "ios/chrome/browser/ui/util/rtl_geometry.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_theme_resources.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const int kRowCount = 6;
const CGFloat kRowHeight = 48.0;
const CGFloat kShortcutsRowHeight = 320;
const CGFloat kAnswerRowHeight = 64.0;
const CGFloat kTopAndBottomPadding = 8.0;
UIColor* BackgroundColorTablet() {
  return [UIColor whiteColor];
}
UIColor* BackgroundColorPhone() {
  return [UIColor colorWithRed:(245 / 255.0)
                         green:(245 / 255.0)
                          blue:(246 / 255.0)
                         alpha:1.0];
}
UIColor* BackgroundColorIncognito() {
  return [UIColor colorWithRed:(50 / 255.0)
                         green:(50 / 255.0)
                          blue:(50 / 255.0)
                         alpha:1.0];
}
}  // namespace

@interface OmniboxPopupViewController ()<UITableViewDelegate,
                                         UITableViewDataSource> {
  // Alignment of omnibox text. Popup text should match this alignment.
  NSTextAlignment _alignment;

  NSArray<id<AutocompleteSuggestion>>* _currentResult;

  // Array containing the OmniboxPopupRow objects displayed in the view.
  NSArray* _rows;

  // The height of the keyboard. Used to determine the content inset for the
  // scroll view.
  CGFloat _keyboardHeight;
}

// Index path of currently highlighted row. The rows can be highlighted by
// tapping and holding on them or by using arrow keys on a hardware keyboard.
@property(nonatomic, strong) NSIndexPath* highlightedIndexPath;

@property(nonatomic, strong) UITableView* tableView;

// Flag that enables forwarding scroll events to the delegate. Disabled while
// updating the cells to avoid defocusing the omnibox when the omnibox popup
// changes size and table view issues a scroll event.
@property(nonatomic, assign) BOOL forwardsScrollEvents;

// The cell with shortcuts to display when no results are available (only if
// this is enabled with |shortcutsEnabled|). Lazily instantiated.
@property(nonatomic, strong) UITableViewCell* shortcutsCell;

@end

@implementation OmniboxPopupViewController
@synthesize delegate = _delegate;
@synthesize incognito = _incognito;
@synthesize imageRetriever = _imageRetriever;
@synthesize highlightedIndexPath = _highlightedIndexPath;
@synthesize tableView = _tableView;
@synthesize forwardsScrollEvents = _forwardsScrollEvents;
#pragma mark -
#pragma mark Initialization

- (instancetype)init {
  if (self = [super initWithNibName:nil bundle:nil]) {
    _forwardsScrollEvents = YES;
    if (IsIPadIdiom()) {
      // The iPad keyboard can cover some of the rows of the scroll view. The
      // scroll view's content inset may need to be updated when the keyboard is
      // displayed.
      NSNotificationCenter* defaultCenter =
          [NSNotificationCenter defaultCenter];
      [defaultCenter addObserver:self
                        selector:@selector(keyboardDidShow:)
                            name:UIKeyboardDidShowNotification
                          object:nil];
    }
  }
  return self;
}

- (void)dealloc {
  self.tableView.delegate = nil;
}

- (void)loadView {
  self.tableView =
      [[SelfSizingTableView alloc] initWithFrame:CGRectZero
                                           style:UITableViewStylePlain];
  self.tableView.delegate = self;
  self.tableView.dataSource = self;
  self.view = self.tableView;
}

- (UIScrollView*)scrollView {
  return (UIScrollView*)self.tableView;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  // Respect the safe area on iOS 11 to support iPhone X.
  if (@available(iOS 11, *)) {
    self.tableView.insetsContentViewsToSafeArea = YES;
  }

  // Initialize the same size as the parent view, autoresize will correct this.
  [self.view setFrame:CGRectZero];
  if (_incognito) {
    self.view.backgroundColor = BackgroundColorIncognito();
  } else {
    self.view.backgroundColor =
        IsIPadIdiom() ? BackgroundColorTablet() : BackgroundColorPhone();
  }

  [self.view setAutoresizingMask:(UIViewAutoresizingFlexibleWidth |
                                  UIViewAutoresizingFlexibleHeight)];

  // Cache fonts needed for omnibox attributed string.
  NSMutableArray* rowsBuilder = [[NSMutableArray alloc] init];
  for (int i = 0; i < kRowCount; i++) {
    OmniboxPopupRow* row =
        [[OmniboxPopupRow alloc] initWithIncognito:_incognito];
    row.accessibilityIdentifier =
        [NSString stringWithFormat:@"omnibox suggestion %i", i];
    row.autoresizingMask = UIViewAutoresizingFlexibleWidth;
    [rowsBuilder addObject:row];
    [row.trailingButton addTarget:self
                           action:@selector(trailingButtonTapped:)
                 forControlEvents:UIControlEventTouchUpInside];
    [row.trailingButton setTag:i];
    row.rowHeight = kRowHeight;
  }
  _rows = [rowsBuilder copy];

  // Table configuration.
  self.tableView.allowsMultipleSelectionDuringEditing = NO;
  self.tableView.separatorStyle = UITableViewCellSeparatorStyleNone;
  self.tableView.separatorInset = UIEdgeInsetsZero;
  if ([self.tableView respondsToSelector:@selector(setLayoutMargins:)]) {
    [self.tableView setLayoutMargins:UIEdgeInsetsZero];
  }
  if (@available(iOS 11, *)) {
    self.tableView.contentInsetAdjustmentBehavior =
        UIScrollViewContentInsetAdjustmentNever;
  }
#if !defined(__IPHONE_11_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_11_0
  else {
    self.automaticallyAdjustsScrollViewInsets = NO;
  }
#endif
  [self.tableView setContentInset:UIEdgeInsetsMake(kTopAndBottomPadding, 0,
                                                   kTopAndBottomPadding, 0)];
  self.tableView.estimatedRowHeight = 0;
}

- (void)didReceiveMemoryWarning {
  [super didReceiveMemoryWarning];
  if (![self isViewLoaded]) {
    _rows = nil;
  }
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  [self layoutRows];

  if (!IsUIRefreshPhase1Enabled()) {
    return;
  }

  ToolbarConfiguration* configuration = [[ToolbarConfiguration alloc]
      initWithStyle:self.incognito ? INCOGNITO : NORMAL];

  if (IsRegularXRegularSizeClass(self)) {
    self.view.backgroundColor = configuration.backgroundColor;
  } else {
    self.view.backgroundColor = [UIColor clearColor];
  }
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
  [coordinator animateAlongsideTransition:^(
                   id<UIViewControllerTransitionCoordinatorContext> context) {
    [self layoutRows];
  }
                               completion:nil];
}

#pragma mark - Properties accessors

- (void)setIncognito:(BOOL)incognito {
  DCHECK(!self.viewLoaded);
  _incognito = incognito;
}

- (void)setShortcutsEnabled:(BOOL)shortcutsEnabled {
  if (shortcutsEnabled == _shortcutsEnabled) {
    return;
  }

  DCHECK(!shortcutsEnabled || self.shortcutsViewController);

  _shortcutsEnabled = shortcutsEnabled;
  [self.tableView reloadData];
}

- (UITableViewCell*)shortcutsCell {
  if (_shortcutsCell) {
    return _shortcutsCell;
  }

  DCHECK(self.shortcutsEnabled);
  DCHECK(self.shortcutsViewController);

  UITableViewCell* cell = [[UITableViewCell alloc] init];
  cell.backgroundColor = [UIColor clearColor];
  [self.shortcutsViewController willMoveToParentViewController:self];
  [self addChildViewController:self.shortcutsViewController];
  [cell.contentView addSubview:self.shortcutsViewController.view];
  self.shortcutsViewController.view.translatesAutoresizingMaskIntoConstraints =
      NO;
  AddSameConstraints(self.shortcutsViewController.view, cell.contentView);
  [self.shortcutsViewController didMoveToParentViewController:self];
  return cell;
}

#pragma mark - AutocompleteResultConsumer

- (void)updateMatches:(NSArray<id<AutocompleteSuggestion>>*)result
        withAnimation:(BOOL)animation {
  self.forwardsScrollEvents = NO;
  // Reset highlight state.
  if (self.highlightedIndexPath) {
    [self unhighlightRowAtIndexPath:self.highlightedIndexPath];
    self.highlightedIndexPath = nil;
  }

  _currentResult = result;
  [self layoutRows];

  size_t size = _currentResult.count;
  if (animation && size > 0) {
    [self fadeInRows];
  }
  self.forwardsScrollEvents = YES;
}

#pragma mark -
#pragma mark Updating data and UI

- (void)updateRow:(OmniboxPopupRow*)row
        withMatch:(id<AutocompleteSuggestion>)match {
  CGFloat kTextCellLeadingPadding =
      [self showsLeadingIcons] ? ([self useRegularWidthOffset] ? 192 : 100)
                               : 16;
  if (IsUIRefreshPhase1Enabled()) {
    kTextCellLeadingPadding = [self showsLeadingIcons] ? 221 : 24;
  }

  const CGFloat kTextCellTopPadding = 6;
  const CGFloat kDetailCellTopPadding = 26;
  const CGFloat kTextLabelHeight = 24;
  const CGFloat kTextDetailLabelHeight = 22;
  const CGFloat kTrailingButtonWidth = 40;
  const CGFloat kAnswerLabelHeight = 36;
  const CGFloat kAnswerImageWidth = 30;
  const CGFloat kAnswerImageLeftPadding = -1;
  const CGFloat kAnswerImageRightPadding = 4;
  const CGFloat kAnswerImageTopPadding = 2;
  const BOOL alignmentRight = _alignment == NSTextAlignmentRight;

  BOOL LTRTextInRTLLayout = _alignment == NSTextAlignmentLeft && UseRTLLayout();

  row.rowHeight = match.hasAnswer ? kAnswerRowHeight : kRowHeight;

  // Fetch the answer image if specified.  Currently, no answer types specify an
  // image on the first line so for now we only look at the second line.
  if (match.hasImage) {
    [self.imageRetriever fetchImage:match.imageURL
                         completion:^(UIImage* image) {
                           row.answerImageView.image = image;
                         }];
    // Answers in suggest do not support RTL, left align only.
    CGFloat imageLeftPadding =
        kTextCellLeadingPadding + kAnswerImageLeftPadding;
    if (alignmentRight) {
      imageLeftPadding =
          row.frame.size.width - (kAnswerImageWidth + kTrailingButtonWidth);
    }
    CGFloat imageTopPadding = kDetailCellTopPadding + kAnswerImageTopPadding;
    row.answerImageView.frame =
        CGRectMake(imageLeftPadding, imageTopPadding, kAnswerImageWidth,
                   kAnswerImageWidth);
    row.answerImageView.hidden = NO;
  } else {
    row.answerImageView.hidden = YES;
  }

  // DetailTextLabel and textLabel are fading labels placed in each row. The
  // textLabel is laid out above the detailTextLabel, and vertically centered
  // if the detailTextLabel is empty.
  // For the detail text label, we use either the regular detail label, which
  // truncates by fading, or the answer label, which uses UILabel's standard
  // truncation by ellipse for the multi-line text sometimes shown in answers.
  row.detailTruncatingLabel.hidden = match.hasAnswer;
  row.detailAnswerLabel.hidden = !match.hasAnswer;
  // URLs have have special layout requirements that need to be invoked here.
  row.detailTruncatingLabel.displayAsURL = match.isURL;

  // TODO(crbug.com/697647): The complexity of managing these two separate
  // labels could probably be encapusulated in the row class if we moved the
  // layout logic there.
  UILabel* detailTextLabel =
      match.hasAnswer ? row.detailAnswerLabel : row.detailTruncatingLabel;
  [detailTextLabel setTextAlignment:_alignment];

  // The width must be positive for CGContextRef to be valid.
  UIEdgeInsets safeAreaInsets = SafeAreaInsetsForView(self.view);
  CGRect rowBounds = UIEdgeInsetsInsetRect(self.view.bounds, safeAreaInsets);
  CGFloat labelWidth =
      MAX(40, floorf(rowBounds.size.width) - kTextCellLeadingPadding);
  CGFloat labelHeight =
      match.hasAnswer ? kAnswerLabelHeight : kTextDetailLabelHeight;
  CGFloat answerImagePadding = kAnswerImageWidth + kAnswerImageRightPadding;
  CGFloat leadingPadding =
      (match.hasImage && !alignmentRight ? answerImagePadding : 0) +
      kTextCellLeadingPadding;

  LayoutRect detailTextLabelLayout =
      LayoutRectMake(leadingPadding, CGRectGetWidth(rowBounds),
                     kDetailCellTopPadding, labelWidth, labelHeight);
  detailTextLabel.frame = LayoutRectGetRect(detailTextLabelLayout);

  detailTextLabel.attributedText = match.detailText;

  // Set detail text label number of lines
  if (match.hasAnswer) {
    detailTextLabel.numberOfLines = match.numberOfLines;
  }

  [detailTextLabel setNeedsDisplay];

  OmniboxPopupTruncatingLabel* textLabel = row.textTruncatingLabel;
  [textLabel setTextAlignment:_alignment];
  LayoutRect textLabelLayout =
      LayoutRectMake(kTextCellLeadingPadding, CGRectGetWidth(rowBounds), 0,
                     labelWidth, kTextLabelHeight);
  textLabel.frame = LayoutRectGetRect(textLabelLayout);

  // Set the text.
  textLabel.attributedText = match.text;

  // Center the textLabel if detailLabel is empty.
  if (!match.hasAnswer && [match.detailText length] == 0) {
    textLabel.center = CGPointMake(textLabel.center.x, floor(kRowHeight / 2));
    textLabel.frame = AlignRectToPixel(textLabel.frame);
  } else {
    CGRect frame = textLabel.frame;
    frame.origin.y = kTextCellTopPadding;
    textLabel.frame = frame;
  }

  // The leading image (e.g. magnifying glass, star, clock) is only shown on
  // iPad.
  if ([self showsLeadingIcons]) {
    UIImage* image = nil;
    if (IsUIRefreshPhase1Enabled()) {
      image = match.suggestionTypeIcon;
    } else {
      image = NativeImage(match.imageID);
    }
    DCHECK(image);
    [row updateLeadingImage:image];
  }

  row.tabMatch = match.isTabMatch;

  // Show append button for search history/search suggestions as the right
  // control element (aka an accessory element of a table view cell).
  row.trailingButton.hidden = !match.isAppendable && !match.isTabMatch;
  [row.trailingButton cancelTrackingWithEvent:nil];

  // If a right accessory element is present or the text alignment is right
  // aligned, adjust the width to align with the accessory element.
  if (match.isAppendable || alignmentRight) {
    LayoutRect layout =
        LayoutRectForRectInBoundingRect(textLabel.frame, self.view.frame);
    layout.size.width -= kTrailingButtonWidth;
    textLabel.frame = LayoutRectGetRect(layout);
    layout =
        LayoutRectForRectInBoundingRect(detailTextLabel.frame, self.view.frame);
    layout.size.width -=
        kTrailingButtonWidth + (match.hasImage ? answerImagePadding : 0);
    detailTextLabel.frame = LayoutRectGetRect(layout);
  }

  // Since it's a common use case to type in a left-to-right URL while the
  // device is set to a native RTL language, make sure the left alignment looks
  // good by anchoring the leading edge to the left.
  if (LTRTextInRTLLayout) {
    // This is really a left padding, not a leading padding.
    const CGFloat kLTRTextInRTLLayoutLeftPadding =
        [self showsLeadingIcons] ? ([self useRegularWidthOffset] ? 176 : 94)
                                 : 94;
    CGRect frame = textLabel.frame;
    frame.size.width -= kLTRTextInRTLLayoutLeftPadding - frame.origin.x;
    frame.origin.x = kLTRTextInRTLLayoutLeftPadding;
    textLabel.frame = frame;

    frame = detailTextLabel.frame;
    frame.size.width -= kLTRTextInRTLLayoutLeftPadding - frame.origin.x;
    frame.origin.x = kLTRTextInRTLLayoutLeftPadding;
    detailTextLabel.frame = frame;
  }

  [textLabel setNeedsDisplay];
}

- (void)layoutRows {
  size_t size = _currentResult.count;

  [self.tableView reloadData];
  [self.tableView beginUpdates];
  for (size_t i = 0; i < kRowCount; i++) {
    OmniboxPopupRow* row = _rows[i];

    if (i < size) {
      [self updateRow:row withMatch:_currentResult[i]];
      row.hidden = NO;
    } else {
      row.hidden = YES;
    }
  }
  [self.tableView endUpdates];

  if (IsIPadIdiom())
    [self updateContentInsetForKeyboard];
}

- (void)keyboardDidShow:(NSNotification*)notification {
  NSDictionary* keyboardInfo = [notification userInfo];
  NSValue* keyboardFrameValue =
      [keyboardInfo valueForKey:UIKeyboardFrameEndUserInfoKey];
  _keyboardHeight = CurrentKeyboardHeight(keyboardFrameValue);
  if (self.tableView.contentSize.height > 0)
    [self updateContentInsetForKeyboard];
}

- (void)updateContentInsetForKeyboard {
  CGRect absoluteRect =
      [self.tableView convertRect:self.tableView.bounds
                toCoordinateSpace:UIScreen.mainScreen.coordinateSpace];
  CGFloat screenHeight = CurrentScreenHeight();
  CGFloat bottomInset = screenHeight - self.tableView.contentSize.height -
                        _keyboardHeight - absoluteRect.origin.y -
                        kTopAndBottomPadding * 2;
  bottomInset = MAX(kTopAndBottomPadding, -bottomInset);
  self.tableView.contentInset =
      UIEdgeInsetsMake(kTopAndBottomPadding, 0, bottomInset, 0);
  self.tableView.scrollIndicatorInsets = self.tableView.contentInset;
}

- (void)fadeInRows {
  [CATransaction begin];
  [CATransaction
      setAnimationTimingFunction:[CAMediaTimingFunction
                                     functionWithControlPoints:0:0:0.2:1]];
  for (size_t i = 0; i < kRowCount; i++) {
    OmniboxPopupRow* row = _rows[i];
    CGFloat beginTime = (i + 1) * .05;
    CABasicAnimation* transformAnimation =
        [CABasicAnimation animationWithKeyPath:@"transform"];
    [transformAnimation
        setFromValue:[NSValue
                         valueWithCATransform3D:CATransform3DMakeTranslation(
                                                    0, -20, 0)]];
    [transformAnimation
        setToValue:[NSValue valueWithCATransform3D:CATransform3DIdentity]];
    [transformAnimation setDuration:0.5];
    [transformAnimation setBeginTime:beginTime];

    CAAnimation* fadeAnimation = OpacityAnimationMake(0, 1);
    [fadeAnimation setDuration:0.5];
    [fadeAnimation setBeginTime:beginTime];

    [[row layer]
        addAnimation:AnimationGroupMake(@[ transformAnimation, fadeAnimation ])
              forKey:@"animateIn"];
  }
  [CATransaction commit];
}

- (void)highlightRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:indexPath];
  [cell setHighlighted:YES animated:NO];
}

- (void)unhighlightRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:indexPath];
  [cell setHighlighted:NO animated:NO];
}

#pragma mark -
#pragma mark Action for append UIButton

- (void)trailingButtonTapped:(id)sender {
  NSUInteger row = [sender tag];
  [self.delegate autocompleteResultConsumer:self
                 didTapTrailingButtonForRow:row];
}

#pragma mark -
#pragma mark UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  // TODO(crbug.com/733650): Default to the dragging check once it's been tested
  // on trunk.
  if (base::ios::IsRunningOnIOS11OrLater()) {
    if (!scrollView.dragging)
      return;
  } else {
    // Setting the top inset of the scrollView to |kTopAndBottomPadding| causes
    // a one time scrollViewDidScroll to |-kTopAndBottomPadding|.  It's easier
    // to just ignore this one scroll tick.
    if (scrollView.contentOffset.y == 0 - kTopAndBottomPadding)
      return;
  }

  if (self.forwardsScrollEvents)
    [self.delegate autocompleteResultConsumerDidScroll:self];
  for (OmniboxPopupRow* row in _rows) {
    row.highlighted = NO;
  }
}

// Set text alignment for popup cells.
- (void)setTextAlignment:(NSTextAlignment)alignment {
  _alignment = alignment;
}

#pragma mark -
#pragma mark OmniboxSuggestionCommands

- (void)highlightNextSuggestion {
  NSIndexPath* path = self.highlightedIndexPath;
  if (path == nil) {
    // When nothing is highlighted, pressing Up Arrow doesn't do anything.
    return;
  }

  if (path.row == 0) {
    // Can't move up from first row. Call the delegate again so that the inline
    // autocomplete text is set again (in case the user exited the inline
    // autocomplete).
    [self.delegate autocompleteResultConsumer:self
                              didHighlightRow:self.highlightedIndexPath.row];
    return;
  }

  [self unhighlightRowAtIndexPath:self.highlightedIndexPath];
  self.highlightedIndexPath =
      [NSIndexPath indexPathForRow:self.highlightedIndexPath.row - 1
                         inSection:0];
  [self highlightRowAtIndexPath:self.highlightedIndexPath];

  [self.delegate autocompleteResultConsumer:self
                            didHighlightRow:self.highlightedIndexPath.row];
}

- (void)highlightPreviousSuggestion {
  if (!self.highlightedIndexPath) {
    // Initialize the highlighted row to -1, so that pressing down when nothing
    // is highlighted highlights the first row (at index 0).
    self.highlightedIndexPath = [NSIndexPath indexPathForRow:-1 inSection:0];
  }

  NSIndexPath* path = self.highlightedIndexPath;

  if (path.row == [self.tableView numberOfRowsInSection:0] - 1) {
    // Can't go below last row. Call the delegate again so that the inline
    // autocomplete text is set again (in case the user exited the inline
    // autocomplete).
    [self.delegate autocompleteResultConsumer:self
                              didHighlightRow:self.highlightedIndexPath.row];
    return;
  }

  // There is a row below, move highlight there.
  [self unhighlightRowAtIndexPath:self.highlightedIndexPath];
  self.highlightedIndexPath =
      [NSIndexPath indexPathForRow:self.highlightedIndexPath.row + 1
                         inSection:0];
  [self highlightRowAtIndexPath:self.highlightedIndexPath];

  [self.delegate autocompleteResultConsumer:self
                            didHighlightRow:self.highlightedIndexPath.row];
}

- (void)keyCommandReturn {
  [self.tableView selectRowAtIndexPath:self.highlightedIndexPath
                              animated:YES
                        scrollPosition:UITableViewScrollPositionNone];
}

#pragma mark -
#pragma mark Table view delegate

- (BOOL)tableView:(UITableView*)tableView
    shouldHighlightRowAtIndexPath:(NSIndexPath*)indexPath {
  if (self.shortcutsEnabled && indexPath.row == 0 &&
      _currentResult.count == 0) {
    return NO;
  }

  return YES;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(0U, (NSUInteger)indexPath.section);
  DCHECK_LT((NSUInteger)indexPath.row, _currentResult.count);
  NSUInteger row = indexPath.row;

  // Crash reports tell us that |row| is sometimes indexed past the end of
  // the results array. In those cases, just ignore the request and return
  // early. See b/5813291.
  if (row >= _currentResult.count)
    return;
  [self.delegate autocompleteResultConsumer:self didSelectRow:row];
}

#pragma mark -
#pragma mark Table view data source

- (CGFloat)tableView:(UITableView*)tableView
    heightForRowAtIndexPath:(NSIndexPath*)indexPath {
  if (self.shortcutsEnabled && indexPath.row == 0 &&
      _currentResult.count == 0) {
    return kShortcutsRowHeight;
  }

  DCHECK_EQ(0U, (NSUInteger)indexPath.section);
  DCHECK_LT((NSUInteger)indexPath.row, _currentResult.count);
  return ((OmniboxPopupRow*)(_rows[indexPath.row])).rowHeight;
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
  return 1;
}

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  DCHECK_EQ(0, section);
  if (self.shortcutsEnabled && _currentResult.count == 0) {
    return 1;
  }
  return _currentResult.count;
}

// Customize the appearance of table view cells.
- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(0U, (NSUInteger)indexPath.section);

  if (self.shortcutsEnabled && indexPath.row == 0 &&
      _currentResult.count == 0) {
    return self.shortcutsCell;
  }

  DCHECK_LT((NSUInteger)indexPath.row, _currentResult.count);
  return _rows[indexPath.row];
}

- (BOOL)tableView:(UITableView*)tableView
    canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(0U, (NSUInteger)indexPath.section);

  if (self.shortcutsEnabled && indexPath.row == 0 &&
      _currentResult.count == 0) {
    return NO;
  }

  // iOS doesn't check -numberOfRowsInSection before checking
  // -canEditRowAtIndexPath in a reload call. If |indexPath.row| is too large,
  // simple return |NO|.
  if ((NSUInteger)indexPath.row >= _currentResult.count)
    return NO;

  return [_currentResult[indexPath.row] supportsDeletion];
}

- (void)tableView:(UITableView*)tableView
    commitEditingStyle:(UITableViewCellEditingStyle)editingStyle
     forRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(0U, (NSUInteger)indexPath.section);
  DCHECK_LT((NSUInteger)indexPath.row, _currentResult.count);
  if (editingStyle == UITableViewCellEditingStyleDelete) {
    // The delete button never disappears if you don't call this after a tap.
    // It doesn't seem to be required anywhere else.
    [_rows[indexPath.row] prepareForReuse];
    [self.delegate autocompleteResultConsumer:self
                      didSelectRowForDeletion:indexPath.row];
  }
}

#pragma mark - private

- (BOOL)showsLeadingIcons {
  if (IsUIRefreshPhase1Enabled()) {
    return IsRegularXRegularSizeClass();
  } else {
    return IsIPadIdiom();
  }
}

- (BOOL)useRegularWidthOffset {
  return [self showsLeadingIcons] && !IsCompactWidth();
}

@end
