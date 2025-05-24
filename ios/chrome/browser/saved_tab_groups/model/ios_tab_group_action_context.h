// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_IOS_TAB_GROUP_ACTION_CONTEXT_H_
#define IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_IOS_TAB_GROUP_ACTION_CONTEXT_H_

#import "base/memory/raw_ptr.h"
#import "components/saved_tab_groups/public/types.h"

class Browser;

namespace tab_groups {

// iOS implementation of TabGroupActionContext used to help with opening saved
// tab groups in the browser.
struct IOSTabGroupActionContext : public TabGroupActionContext {
  explicit IOSTabGroupActionContext(Browser* browser);
  ~IOSTabGroupActionContext() override = default;

  raw_ptr<Browser> browser;
};

}  // namespace tab_groups

#endif  // IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_MODEL_IOS_TAB_GROUP_ACTION_CONTEXT_H_
