// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/disabled_grid_view_controller.h"

#import "components/supervised_user/core/common/supervised_user_constants.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_paging.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/text_view_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {
const CGFloat kVerticalMargin = 16.0;
}  // namespace

@interface DisabledGridViewController () <UITextViewDelegate>

@property(nonatomic, strong) UIScrollView* scrollView;
@property(nonatomic, strong) NSLayoutConstraint* scrollViewHeight;
@property(nonatomic, assign) UIEdgeInsets scrollViewContentInsets;
@property(nonatomic, assign) TabGridPage page;

@end

@implementation DisabledGridViewController

namespace {
// Create a NSString for the title based on `page`.
NSString* GetTitleString(TabGridPage page) {
  switch (page) {
    case TabGridPageIncognitoTabs:
      return l10n_util::GetNSString(
          IDS_IOS_TAB_GRID_INCOGNITO_TABS_UNAVAILABLE_TITLE);
    case TabGridPageRegularTabs:
      return l10n_util::GetNSString(
          IDS_IOS_TAB_GRID_REGULAR_TABS_UNAVAILABLE_TITLE);
    case TabGridPageRemoteTabs:
      return l10n_util::GetNSString(
          IDS_IOS_TAB_GRID_RECENT_TABS_UNAVAILABLE_TITLE);
    case TabGridPageTabGroups:
      return l10n_util::GetNSString(
          IDS_IOS_TAB_GRID_TAB_GROUPS_UNAVAILABLE_TITLE);
  }
}
}  // namespace

- (instancetype)initWithPage:(TabGridPage)page {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _page = page;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  UIScrollView* scrollView = [[UIScrollView alloc] init];
  scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  self.scrollView = scrollView;

  UIView* container = [[UIView alloc] init];
  container.translatesAutoresizingMaskIntoConstraints = NO;

  UILabel* topLabel = [[UILabel alloc] init];
  topLabel.translatesAutoresizingMaskIntoConstraints = NO;
  topLabel.text = GetTitleString(self.page);
  topLabel.textColor = UIColorFromRGB(kTabGridEmptyStateTitleTextColor);
  topLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];
  topLabel.adjustsFontForContentSizeCategory = YES;
  topLabel.numberOfLines = 0;
  topLabel.textAlignment = NSTextAlignmentCenter;

  UITextView* bottomTextView = CreateUITextViewWithTextKit1();
  bottomTextView.translatesAutoresizingMaskIntoConstraints = NO;
  bottomTextView.attributedText = self.messageBodyAttributedString;
  bottomTextView.scrollEnabled = NO;
  bottomTextView.editable = NO;
  bottomTextView.delegate = self;
  bottomTextView.backgroundColor = [UIColor colorNamed:kGridBackgroundColor];
  bottomTextView.textColor = UIColorFromRGB(kTabGridEmptyStateBodyTextColor);
  bottomTextView.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  bottomTextView.adjustsFontForContentSizeCategory = YES;
  bottomTextView.textAlignment = NSTextAlignmentCenter;

  [container addSubview:topLabel];
  [container addSubview:bottomTextView];
  [scrollView addSubview:container];
  [self.view addSubview:scrollView];

  self.scrollViewHeight = VerticalConstraintsWithInset(
      container, scrollView,
      self.scrollViewContentInsets.top + self.scrollViewContentInsets.bottom);

  [NSLayoutConstraint activateConstraints:@[
    [topLabel.topAnchor constraintEqualToAnchor:container.topAnchor
                                       constant:kVerticalMargin],
    [topLabel.leadingAnchor constraintEqualToAnchor:container.leadingAnchor],
    [topLabel.trailingAnchor constraintEqualToAnchor:container.trailingAnchor],
    [topLabel.bottomAnchor
        constraintEqualToAnchor:bottomTextView.topAnchor
                       constant:-kTabGridEmptyStateVerticalMargin],

    [bottomTextView.leadingAnchor
        constraintEqualToAnchor:container.leadingAnchor],
    [bottomTextView.trailingAnchor
        constraintEqualToAnchor:container.trailingAnchor],
    [bottomTextView.bottomAnchor constraintEqualToAnchor:container.bottomAnchor
                                                constant:-kVerticalMargin],

    [container.topAnchor constraintEqualToAnchor:scrollView.topAnchor],
    [container.bottomAnchor constraintEqualToAnchor:scrollView.bottomAnchor],
    [container.widthAnchor
        constraintLessThanOrEqualToAnchor:self.view.safeAreaLayoutGuide
                                              .widthAnchor],
    [container.centerXAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.centerXAnchor],
    [scrollView.centerYAnchor constraintEqualToAnchor:self.view.centerYAnchor],
    [scrollView.topAnchor
        constraintGreaterThanOrEqualToAnchor:self.view.topAnchor],
    [scrollView.bottomAnchor
        constraintLessThanOrEqualToAnchor:self.view.bottomAnchor],
    [scrollView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [scrollView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
  ]];
}

#pragma mark - UITextViewDelegate

- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange
              interaction:(UITextItemInteraction)interaction {
  if (URL) {
    [self.delegate didTapLinkWithURL:net::GURLWithNSURL(URL)];
  }
  // Return NO as the app is handling the opening of the URL.
  return NO;
}

#pragma mark - Accessor

- (void)setScrollViewContentInsets:(UIEdgeInsets)scrollViewContentInsets {
  _scrollViewContentInsets = scrollViewContentInsets;
  self.scrollView.contentInset = scrollViewContentInsets;
  self.scrollViewHeight.constant =
      scrollViewContentInsets.top + scrollViewContentInsets.bottom;
}

// Creates an attribute string with link for the body message.
- (NSAttributedString*)messageBodyAttributedString {
  CHECK(self.delegate);
  int messageID;

  BOOL isSubjectToParentalControls =
      self.delegate.isViewControllerSubjectToParentalControls;
  switch (self.page) {
    case TabGridPageIncognitoTabs:
      if (isSubjectToParentalControls) {
        messageID = IDS_IOS_TAB_GRID_SUPERVISED_INCOGNITO_MESSAGE;
      } else {
        messageID = IDS_IOS_TAB_GRID_INCOGNITO_TABS_UNAVAILABLE_MESSAGE;
      }
      break;
    case TabGridPageRegularTabs:
      messageID = IDS_IOS_TAB_GRID_REGULAR_TABS_UNAVAILABLE_MESSAGE;
      break;
    case TabGridPageRemoteTabs:
      messageID = IDS_IOS_TAB_GRID_RECENT_TABS_UNAVAILABLE_MESSAGE;
      break;
    case TabGridPageTabGroups:
      messageID = IDS_IOS_TAB_GRID_TAB_GROUPS_UNAVAILABLE_MESSAGE;
      break;
  }

  const std::string learnMoreURL =
      isSubjectToParentalControls
          ? supervised_user::kManagedByParentUiMoreInfoUrl
          : kChromeUIManagementURL;

  NSString* fullText = l10n_util::GetNSString(messageID);

  // Sets the styling to mimic a link.
  NSDictionary* linkAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor],
    NSLinkAttributeName : [NSString stringWithUTF8String:learnMoreURL.c_str()],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote],
    NSUnderlineStyleAttributeName : @(NSUnderlineStyleNone),
  };

  return AttributedStringFromStringWithLink(fullText, @{}, linkAttributes);
}

@end
