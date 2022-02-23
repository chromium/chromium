// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/bookmarks/bookmark_ios_unittest.h"
#include <memory>

#include "base/strings/sys_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service_fake.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using bookmarks::BookmarkNode;

BookmarkIOSUnitTest::BookmarkIOSUnitTest() {}
BookmarkIOSUnitTest::~BookmarkIOSUnitTest() {}

void BookmarkIOSUnitTest::SetUp() {
  // Get a BookmarkModel from the test ChromeBrowserState.
  TestChromeBrowserState::Builder test_cbs_builder;

  test_cbs_builder.AddTestingFactory(
      AuthenticationServiceFactory::GetInstance(),
      base::BindRepeating(
          &AuthenticationServiceFake::CreateAuthenticationService));
  test_cbs_builder.AddTestingFactory(
      ios::BookmarkModelFactory::GetInstance(),
      ios::BookmarkModelFactory::GetDefaultFactory());

  chrome_browser_state_ = test_cbs_builder.Build();

  bookmark_model_ = ios::BookmarkModelFactory::GetForBrowserState(
      chrome_browser_state_.get());
  bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model_);
  browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get());
}

const BookmarkNode* BookmarkIOSUnitTest::AddBookmark(const BookmarkNode* parent,
                                                     NSString* title) {
  std::u16string c_title = base::SysNSStringToUTF16(title);
  GURL url(base::SysNSStringToUTF16(@"http://example.com/bookmark") + c_title);
  return bookmark_model_->AddURL(parent, parent->children().size(), c_title,
                                 url);
}

const BookmarkNode* BookmarkIOSUnitTest::AddFolder(const BookmarkNode* parent,
                                                   NSString* title) {
  std::u16string c_title = base::SysNSStringToUTF16(title);
  return bookmark_model_->AddFolder(parent, parent->children().size(), c_title);
}

void BookmarkIOSUnitTest::ChangeTitle(NSString* title,
                                      const BookmarkNode* node) {
  std::u16string c_title = base::SysNSStringToUTF16(title);
  bookmark_model_->SetTitle(node, c_title);
}
