// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_shared_tabs_view_controller.h"

#import <UIKit/UIKit.h>

#import <optional>

#import "base/apple/foundation_util.h"
#import "base/unguessable_token.h"
#import "ios/chrome/browser/composebox/menu/coordinator/composebox_menu_shared_tab.h"
#import "ios/chrome/browser/composebox/shared/ui/composebox_ui_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

// Expected line limits for the tab title and domain labels (0 = unlimited).
const NSInteger kExpectedTitleNumberOfLines = 0;
const NSInteger kExpectedDomainNumberOfLines = 0;

// Sub-point tolerance for comparing laid-out label heights against
// `sizeThatFits:` on 3x scale screens.
const CGFloat kHeightComparisonTolerance = 0.5;

// Test tab titles and URLs.
NSString* const kShortTabTitle = @"Tab";
constexpr char kShortTabURL[] = "https://x.org/page";
NSString* const kLongTabTitle =
    @"Dynamic Type audit page with a very long title that wraps across "
    @"multiple lines at accessibility text sizes";
constexpr char kLongTabURL[] =
    "https://very-long-subdomain-for-dynamic-type.example.com/page";
NSString* const kExpectedLongDomain =
    @"very-long-subdomain-for-dynamic-type.example.com";

}  // namespace

// Fake delegate recording `ComposeboxMenuSharedTabsViewController` actions.
@interface FakeComposeboxMenuSharedTabsDelegate
    : NSObject <ComposeboxMenuSharedTabsViewControllerDelegate>

// Identifier of the last tab removed via the view controller, if any.
@property(nonatomic, readonly) std::optional<base::UnguessableToken>
    removedInputItemIdentifier;

@end

@implementation FakeComposeboxMenuSharedTabsDelegate

#pragma mark - ComposeboxMenuSharedTabsViewControllerDelegate

- (void)composeboxMenuSharedTabsViewController:
            (ComposeboxMenuSharedTabsViewController*)viewController
                                     didTapURL:(const GURL&)url {
}

- (void)composeboxMenuSharedTabsViewController:
            (ComposeboxMenuSharedTabsViewController*)viewController
           didRemoveTabWithInputItemIdentifier:
               (const base::UnguessableToken&)inputItemIdentifier {
  _removedInputItemIdentifier = inputItemIdentifier;
}

@end

class ComposeboxMenuSharedTabsViewControllerTest : public PlatformTest {
 protected:
  ComposeboxMenuSharedTabsViewControllerTest() { SetSharedTabs(@[]); }

  // Recreates the view controller with `shared_tabs` and hosts it in the key
  // window so live Dynamic Type updates and collection layout work.
  void SetSharedTabs(NSArray<ComposeboxMenuSharedTab*>* shared_tabs) {
    view_controller_ = [[ComposeboxMenuSharedTabsViewController alloc]
        initWithSharedTabs:shared_tabs];
    scoped_key_window_.Get().rootViewController = view_controller_;
  }

  // Sets the content size category of the window hosting the view controller
  // and lays it out, as if the user changed the text size.
  void SetContentSizeCategory(UIContentSizeCategory category) {
    UIWindow* window = scoped_key_window_.Get();
    window.traitOverrides.preferredContentSizeCategory = category;
    [window layoutIfNeeded];
  }

  // Returns the collection view displaying the shared tabs.
  UICollectionView* CollectionView() {
    return chrome_test_util::FindViewByClass<UICollectionView>(
        view_controller_.view);
  }

  // Returns the disclaimer text view.
  UITextView* DisclaimerTextView() {
    return chrome_test_util::FindViewById<UITextView>(
        view_controller_.view,
        kComposeboxSharedTabsDisclaimerAccessibilityIdentifier);
  }

  // Returns the font of the disclaimer's attributed text at `index`.
  UIFont* DisclaimerFontAtIndex(NSUInteger index) {
    return [DisclaimerTextView().attributedText attribute:NSFontAttributeName
                                                  atIndex:index
                                           effectiveRange:nullptr];
  }

  ScopedKeyWindow scoped_key_window_;
  ComposeboxMenuSharedTabsViewController* view_controller_;
};

// Test that the disclaimer font grows when the text size changes while the
// Shared Tabs sheet is on screen.
TEST_F(ComposeboxMenuSharedTabsViewControllerTest,
       DisclaimerScalesOnLiveContentSizeChange) {
  SetContentSizeCategory(UIContentSizeCategoryLarge);
  ASSERT_TRUE(DisclaimerTextView());
  ASSERT_GT(DisclaimerTextView().attributedText.length, 0u);
  UIFont* large_font = DisclaimerFontAtIndex(0);
  ASSERT_TRUE(large_font);

  SetContentSizeCategory(
      UIContentSizeCategoryAccessibilityExtraExtraExtraLarge);
  EXPECT_GT(DisclaimerFontAtIndex(0).pointSize, large_font.pointSize);
}

// Test that the "Learn more" link survives a text size change.
TEST_F(ComposeboxMenuSharedTabsViewControllerTest,
       DisclaimerKeepsLinkOnLiveContentSizeChange) {
  SetContentSizeCategory(UIContentSizeCategoryLarge);
  ASSERT_TRUE(DisclaimerTextView());
  NSAttributedString* text = DisclaimerTextView().attributedText;
  ASSERT_GT(text.length, 0u);
  // The link is at the end of the disclaimer.
  id large_link = [text attribute:NSLinkAttributeName
                          atIndex:text.length - 1
                   effectiveRange:nullptr];
  ASSERT_TRUE(large_link);

  SetContentSizeCategory(
      UIContentSizeCategoryAccessibilityExtraExtraExtraLarge);
  text = DisclaimerTextView().attributedText;
  ASSERT_GT(text.length, 0u);
  EXPECT_NSEQ(large_link, [text attribute:NSLinkAttributeName
                                     atIndex:text.length - 1
                              effectiveRange:nullptr]);
}

// Tests that Shared Tabs rows allow unlimited lines for both the tab title and
// domain, and that cells self-size without clipping at AX5.
TEST_F(ComposeboxMenuSharedTabsViewControllerTest,
       SharedTabRowsWrapTitleAndDomainAtAccessibilitySizes) {
  ComposeboxMenuSharedTab* short_tab = [[ComposeboxMenuSharedTab alloc]
              initWithURL:GURL(kShortTabURL)
                    title:kShortTabTitle
      inputItemIdentifier:base::UnguessableToken::Create()
                  favicon:nil];
  ComposeboxMenuSharedTab* long_tab = [[ComposeboxMenuSharedTab alloc]
              initWithURL:GURL(kLongTabURL)
                    title:kLongTabTitle
      inputItemIdentifier:base::UnguessableToken::Create()
                  favicon:nil];
  SetSharedTabs(@[ short_tab, long_tab ]);
  SetContentSizeCategory(
      UIContentSizeCategoryAccessibilityExtraExtraExtraLarge);

  UICollectionView* collection_view = CollectionView();
  ASSERT_TRUE(collection_view);
  [collection_view layoutIfNeeded];

  UICollectionViewListCell* short_cell =
      base::apple::ObjCCast<UICollectionViewListCell>([collection_view
          cellForItemAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:0]]);
  UICollectionViewListCell* long_cell =
      base::apple::ObjCCast<UICollectionViewListCell>([collection_view
          cellForItemAtIndexPath:[NSIndexPath indexPathForItem:1 inSection:0]]);
  ASSERT_TRUE(short_cell);
  ASSERT_TRUE(long_cell);

  UIListContentConfiguration* content =
      base::apple::ObjCCast<UIListContentConfiguration>(
          long_cell.contentConfiguration);
  ASSERT_TRUE(content);
  EXPECT_EQ(content.textProperties.numberOfLines, kExpectedTitleNumberOfLines);
  EXPECT_EQ(content.secondaryTextProperties.numberOfLines,
            kExpectedDomainNumberOfLines);

  // The multi-line row must self-size taller than the single-line row.
  EXPECT_GT(CGRectGetHeight(long_cell.bounds),
            CGRectGetHeight(short_cell.bounds));

  UILabel* title_label =
      chrome_test_util::FindLabelWithText(long_cell, kLongTabTitle);
  ASSERT_TRUE(title_label);
  EXPECT_EQ(title_label.numberOfLines, kExpectedTitleNumberOfLines);
  EXPECT_GT(CGRectGetHeight(title_label.bounds), title_label.font.lineHeight);
  CGFloat required_title_height =
      [title_label sizeThatFits:CGSizeMake(CGRectGetWidth(title_label.bounds),
                                           CGFLOAT_MAX)]
          .height;
  EXPECT_GE(CGRectGetHeight(title_label.bounds),
            required_title_height - kHeightComparisonTolerance);

  UILabel* domain_label =
      chrome_test_util::FindLabelWithText(long_cell, kExpectedLongDomain);
  ASSERT_TRUE(domain_label);
  EXPECT_EQ(domain_label.numberOfLines, kExpectedDomainNumberOfLines);
  EXPECT_GT(CGRectGetHeight(domain_label.bounds), domain_label.font.lineHeight);
  CGFloat required_domain_height =
      [domain_label sizeThatFits:CGSizeMake(CGRectGetWidth(domain_label.bounds),
                                            CGFLOAT_MAX)]
          .height;
  EXPECT_GE(CGRectGetHeight(domain_label.bounds),
            required_domain_height - kHeightComparisonTolerance);
}

// Tests that the shared tabs collection view disables item selection so
// double-tapping a row in VoiceOver does not leave it in a selected state.
TEST_F(ComposeboxMenuSharedTabsViewControllerTest,
       CollectionViewDisablesSelection) {
  UICollectionView* collection_view = CollectionView();
  ASSERT_TRUE(collection_view);

  EXPECT_FALSE(collection_view.allowsSelection);
}

// Tests that invoking the Remove Tab accessibility custom action on a shared
// tab cell removes the tab from the collection view and notifies the delegate.
TEST_F(ComposeboxMenuSharedTabsViewControllerTest,
       RemoveTabAccessibilityActionRemovesItemAndNotifiesDelegate) {
  const base::UnguessableToken first_identifier =
      base::UnguessableToken::Create();
  const base::UnguessableToken second_identifier =
      base::UnguessableToken::Create();
  ComposeboxMenuSharedTab* first_tab = [[ComposeboxMenuSharedTab alloc]
              initWithURL:GURL("https://first.example.org")
                    title:@"First Tab"
      inputItemIdentifier:first_identifier
                  favicon:nil];
  ComposeboxMenuSharedTab* second_tab = [[ComposeboxMenuSharedTab alloc]
              initWithURL:GURL("https://second.example.org")
                    title:@"Second Tab"
      inputItemIdentifier:second_identifier
                  favicon:nil];
  SetSharedTabs(@[ first_tab, second_tab ]);

  FakeComposeboxMenuSharedTabsDelegate* delegate =
      [[FakeComposeboxMenuSharedTabsDelegate alloc] init];
  view_controller_.delegate = delegate;

  UICollectionView* collection_view = CollectionView();
  ASSERT_TRUE(collection_view);
  [collection_view layoutIfNeeded];

  UICollectionViewListCell* first_cell =
      base::apple::ObjCCast<UICollectionViewListCell>([collection_view
          cellForItemAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:0]]);
  ASSERT_TRUE(first_cell);
  EXPECT_TRUE(chrome_test_util::FindLabelWithText(first_cell, @"First Tab"));
  ASSERT_EQ(first_cell.accessibilityCustomActions.count, 1u);
  UIAccessibilityCustomAction* remove_action =
      first_cell.accessibilityCustomActions[0];
  ASSERT_NSEQ(remove_action.name,
              l10n_util::GetNSString(IDS_IOS_COMPOSEBOX_MENU_REMOVE_TAB));
  ASSERT_TRUE(remove_action.actionHandler);

  BOOL handled = remove_action.actionHandler(remove_action);

  EXPECT_TRUE(handled);
  EXPECT_EQ([collection_view numberOfItemsInSection:0], 1);
  [collection_view layoutIfNeeded];
  UICollectionViewListCell* remaining_cell =
      base::apple::ObjCCast<UICollectionViewListCell>([collection_view
          cellForItemAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:0]]);
  ASSERT_TRUE(remaining_cell);
  EXPECT_TRUE(
      chrome_test_util::FindLabelWithText(remaining_cell, @"Second Tab"));
  EXPECT_FALSE(
      chrome_test_util::FindLabelWithText(remaining_cell, @"First Tab"));
  ASSERT_TRUE(delegate.removedInputItemIdentifier.has_value());
  EXPECT_EQ(delegate.removedInputItemIdentifier.value(), first_identifier);
}
