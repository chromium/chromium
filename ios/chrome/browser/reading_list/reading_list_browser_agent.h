// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READING_LIST_READING_LIST_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_READING_LIST_READING_LIST_BROWSER_AGENT_H_

#import "base/scoped_multi_source_observation.h"
#import "base/scoped_observation.h"
#import "ios/chrome/browser/main/browser_user_data.h"
#import "ios/chrome/browser/shared/ui/util/url_with_title.h"

class Browser;

class ReadingListBrowserAgent
    : public BrowserUserData<ReadingListBrowserAgent> {
 public:
  ~ReadingListBrowserAgent() override;

  // Not copyable or assignable.
  ReadingListBrowserAgent(const ReadingListBrowserAgent&) = delete;
  ReadingListBrowserAgent& operator=(const ReadingListBrowserAgent&) = delete;
  void AddURLsToReadingList(NSArray<URLWithTitle*>* URLs);

 private:
  friend class BrowserUserData<ReadingListBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  explicit ReadingListBrowserAgent(Browser* browser);

  void AddURLToReadingListwithTitle(const GURL& URL, NSString* title);

  // The browser associated with this agent.
  Browser* browser_;
};

#endif  // IOS_CHROME_BROWSER_READING_LIST_READING_LIST_BROWSER_AGENT_H_
