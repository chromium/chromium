// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARK_IOS_UNIT_TEST_SUPPORT_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARK_IOS_UNIT_TEST_SUPPORT_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "ios/chrome/browser/shared/model/profile/test/test_profile_ios_forward.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/platform_test.h"

class Browser;
class GURL;
class PrefService;

namespace bookmarks {
class BookmarkNode;
class BookmarkModel;
class ManagedBookmarkService;
}  // namespace bookmarks

// Provides common bookmark testing infrastructure.
class BookmarkIOSUnitTestSupport : public PlatformTest {
 public:
  explicit BookmarkIOSUnitTestSupport(bool wait_for_initialization = true);
  ~BookmarkIOSUnitTestSupport() override;

 protected:
  void SetUp() override;

  // Allows subclasses to add custom logic immediately following the creation
  // of the BrowserState, before keyed services are created.
  virtual void SetUpBrowserStateBeforeCreatingServices();

  const bookmarks::BookmarkNode* AddBookmark(
      const bookmarks::BookmarkNode* parent,
      const std::u16string& title);
  const bookmarks::BookmarkNode* AddBookmark(
      const bookmarks::BookmarkNode* parent,
      const std::u16string& title,
      const GURL& url);
  const bookmarks::BookmarkNode* AddFolder(
      const bookmarks::BookmarkNode* parent,
      const std::u16string& title);
  void ChangeTitle(const std::u16string& title,
                   const bookmarks::BookmarkNode* node);

  const bool wait_for_initialization_;
  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<Browser> browser_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<bookmarks::BookmarkModel> bookmark_model_;
  raw_ptr<bookmarks::ManagedBookmarkService> managed_bookmark_service_;
  raw_ptr<PrefService> pref_service_;
};

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_MODEL_BOOKMARK_IOS_UNIT_TEST_SUPPORT_H_
