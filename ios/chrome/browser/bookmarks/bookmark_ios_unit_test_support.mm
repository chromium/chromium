// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/bookmark_ios_unit_test_support.h"
#import <memory>

#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/common/bookmark_metrics.h"
#import "components/bookmarks/test/bookmark_test_helpers.h"
#import "ios/chrome/browser/bookmarks/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using bookmarks::BookmarkNode;

BookmarkIOSUnitTestSupport::BookmarkIOSUnitTestSupport() {}
BookmarkIOSUnitTestSupport::~BookmarkIOSUnitTestSupport() {}

void BookmarkIOSUnitTestSupport::SetUp() {
  // Get a BookmarkModel from the test ChromeBrowserState.
  TestChromeBrowserState::Builder test_cbs_builder;
  test_cbs_builder.AddTestingFactory(
      AuthenticationServiceFactory::GetInstance(),
      AuthenticationServiceFactory::GetDefaultFactory());
  test_cbs_builder.AddTestingFactory(
      ios::LocalOrSyncableBookmarkModelFactory::GetInstance(),
      ios::LocalOrSyncableBookmarkModelFactory::GetDefaultFactory());
  test_cbs_builder.AddTestingFactory(
      ManagedBookmarkServiceFactory::GetInstance(),
      ManagedBookmarkServiceFactory::GetDefaultFactory());

  chrome_browser_state_ = test_cbs_builder.Build();
  AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
      chrome_browser_state_.get(),
      std::make_unique<FakeAuthenticationServiceDelegate>());

  profile_bookmark_model_ =
      ios::LocalOrSyncableBookmarkModelFactory::GetForBrowserState(
          chrome_browser_state_.get());
  bookmarks::test::WaitForBookmarkModelToLoad(profile_bookmark_model_);
  browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get());
}

const BookmarkNode* BookmarkIOSUnitTestSupport::AddBookmark(
    const BookmarkNode* parent,
    NSString* title) {
  std::u16string c_title = base::SysNSStringToUTF16(title);
  GURL url(base::SysNSStringToUTF16(@"http://example.com/bookmark") + c_title);
  return profile_bookmark_model_->AddURL(parent, parent->children().size(),
                                         c_title, url);
}

const BookmarkNode* BookmarkIOSUnitTestSupport::AddFolder(
    const BookmarkNode* parent,
    NSString* title) {
  std::u16string c_title = base::SysNSStringToUTF16(title);
  return profile_bookmark_model_->AddFolder(parent, parent->children().size(),
                                            c_title);
}

void BookmarkIOSUnitTestSupport::ChangeTitle(NSString* title,
                                             const BookmarkNode* node) {
  std::u16string c_title = base::SysNSStringToUTF16(title);
  profile_bookmark_model_->SetTitle(
      node, c_title, bookmarks::metrics::BookmarkEditSource::kUser);
}
