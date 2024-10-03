// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sad_tab/ui_bundled/sad_tab_view_controller.h"

#import <CoreGraphics/CoreGraphics.h>

#import "ios/chrome/browser/overscroll_actions/ui_bundled/overscroll_actions_controller.h"
#import "ios/chrome/browser/sad_tab/ui_bundled/sad_tab_view.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "url/gurl.h"

@interface SadTabViewController ()<SadTabViewDelegate>

@property(nonatomic) SadTabView* sadTabView;

// Scroll view is required by OverscrollActionsController and will be a parent
// view of the sad tab view.
@property(nonatomic) UIScrollView* scrollView;

// Allows supporting Overscroll Actions UI, which is displayed when Sad Tab is
// pulled down.
@property(nonatomic) OverscrollActionsController* overscrollActionsController;

@end

@implementation SadTabViewController

@synthesize repeatedFailure = _repeatedFailure;
@synthesize offTheRecord = _offTheRecord;
@synthesize sadTabView = _sadTabView;
@synthesize scrollView = _scrollView;
@synthesize overscrollActionsController = _overscrollActionsController;
@synthesize overscrollDelegate = _overscrollDelegate;

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];

  // Scroll view is required by OverscrollActionsController and will be a parent
  // view of the sad tab view.
  self.scrollView = [[UIScrollView alloc] init];
  self.scrollView.showsVerticalScrollIndicator = NO;
  [self.view addSubview:self.scrollView];
  self.scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(self.scrollView, self.view);

  // SadTabView is a child of the scroll view as reqired by
  // OverscrollActionsController.
  SadTabViewMode mode =
      self.repeatedFailure ? SadTabViewMode::FEEDBACK : SadTabViewMode::RELOAD;
  self.sadTabView =
      [[SadTabView alloc] initWithMode:mode offTheRecord:self.offTheRecord];
  self.sadTabView.delegate = self;
  [self.scrollView addSubview:self.sadTabView];

  // OverscrollActionsController allows Overscroll Actions UI.
  self.overscrollActionsController =
      [[OverscrollActionsController alloc] initWithScrollView:self.scrollView];
  self.overscrollActionsController.delegate = self.overscrollDelegate;
  self.scrollView.delegate = self.overscrollActionsController;
  OverscrollStyle style = self.offTheRecord
                              ? OverscrollStyle::REGULAR_PAGE_INCOGNITO
                              : OverscrollStyle::REGULAR_PAGE_NON_INCOGNITO;
  [self.overscrollActionsController setStyle:style];
  [self updateOverscrollActionsState];

  if (@available(iOS 17, *)) {
    NSArray<UITrait>* traits = TraitCollectionSetForTraits(
        @[ UITraitHorizontalSizeClass.self, UITraitVerticalSizeClass.self ]);
    [self registerForTraitChanges:traits
                       withAction:@selector(updateOverscrollActionsState)];
  }
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];

  // In order to allow UIScollView to scroll vertically, the height of the
  // content should be taller than the hight of the scroll view.
  CGRect newFrame = self.view.bounds;
  newFrame.size.height += 1;
  self.sadTabView.frame = newFrame;
  [self.scrollView setContentSize:newFrame.size];
}

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }

  [self updateOverscrollActionsState];
}
#endif

#pragma mark - SadTabViewDelegate

- (void)sadTabViewShowReportAnIssue:(SadTabView*)sadTabView {
  [self.delegate sadTabViewControllerShowReportAnIssue:self];
}

- (void)sadTabView:(SadTabView*)sadTabView
    showSuggestionsPageWithURL:(const GURL&)URL {
  [self.delegate sadTabViewController:self showSuggestionsPageWithURL:URL];
}

- (void)sadTabViewReload:(SadTabView*)sadTabView {
  [self.delegate sadTabViewControllerReload:self];
}

#pragma mark - Private

// Enables or disables overscroll actions.
- (void)updateOverscrollActionsState {
  if (IsSplitToolbarMode(self)) {
    [self.overscrollActionsController enableOverscrollActions];
  } else {
    [self.overscrollActionsController disableOverscrollActions];
  }
}

@end

#pragma mark -

@implementation SadTabViewController (UIElements)

- (UITextView*)messageTextView {
  return self.sadTabView.messageTextView;
}

- (UIButton*)actionButton {
  return self.sadTabView.actionButton;
}

@end
