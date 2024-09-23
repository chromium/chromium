// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/model/bookmark_ios_unit_test_support.h"

#import <memory>

#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/common/bookmark_features.h"
#import "components/bookmarks/common/bookmark_metrics.h"
#import "components/bookmarks/test/bookmark_test_helpers.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/bookmarks/model/managed_bookmark_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "url/gurl.h"

using bookmarks::BookmarkNode;

BookmarkIOSUnitTestSupport::BookmarkIOSUnitTestSupport(
    bool wait_for_initialization)
    : wait_for_initialization_(wait_for_initialization) {}

BookmarkIOSUnitTestSupport::~BookmarkIOSUnitTestSupport() = default;

void BookmarkIOSUnitTestSupport::SetUp() {
  // Get a BookmarkModel from the test ProfileIOS.
  TestProfileIOS::Builder test_profile_builder;
  test_profile_builder.AddTestingFactory(
      AuthenticationServiceFactory::GetInstance(),
      AuthenticationServiceFactory::GetDefaultFactory());
  test_profile_builder.AddTestingFactory(
      ios::BookmarkModelFactory::GetInstance(),
      ios::BookmarkModelFactory::GetDefaultFactory());
  test_profile_builder.AddTestingFactory(
      ManagedBookmarkServiceFactory::GetInstance(),
      ManagedBookmarkServiceFactory::GetDefaultFactory());

  profile_ = std::move(test_profile_builder).Build();

  SetUpBrowserStateBeforeCreatingServices();

  AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
      profile_.get(), std::make_unique<FakeAuthenticationServiceDelegate>());

  bookmark_model_ = ios::BookmarkModelFactory::GetForProfile(profile_.get());
  if (wait_for_initialization_) {
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model_);
  }

  pref_service_ = profile_->GetPrefs();
  EXPECT_TRUE(pref_service_);

  if (wait_for_initialization_) {
    // Some tests exercise account bookmarks. Make sure their permanent
    // folders exist.
    ios::BookmarkModelFactory::GetForProfile(profile_.get())
        ->CreateAccountPermanentFolders();
  }

  browser_ = std::make_unique<TestBrowser>(profile_.get());
}

void BookmarkIOSUnitTestSupport::SetUpBrowserStateBeforeCreatingServices() {}

const BookmarkNode* BookmarkIOSUnitTestSupport::AddBookmark(
    const BookmarkNode* parent,
    const std::u16string& title) {
  GURL url(u"http://example.com/bookmark" + title);
  return AddBookmark(parent, title, url);
}

const BookmarkNode* BookmarkIOSUnitTestSupport::AddBookmark(
    const BookmarkNode* parent,
    const std::u16string& title,
    const GURL& url) {
  return bookmark_model_->AddURL(parent, parent->children().size(), title, url);
}

const BookmarkNode* BookmarkIOSUnitTestSupport::AddFolder(
    const BookmarkNode* parent,
    const std::u16string& title) {
  return bookmark_model_->AddFolder(parent, parent->children().size(), title);
}

void BookmarkIOSUnitTestSupport::ChangeTitle(const std::u16string& title,
                                             const BookmarkNode* node) {
  bookmark_model_->SetTitle(node, title,
                            bookmarks::metrics::BookmarkEditSource::kUser);
}
