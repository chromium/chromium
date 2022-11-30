// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_BUDGET_CHARGER_H_
#define CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_BUDGET_CHARGER_H_

#include "content/common/content_export.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {

// Class responsible for observing top navigations initiated from fenced frames
// originated (directly or indirectly) from shared storage url selection. In
// that case, we will charge the privacy budget to the shared storage origin.
// This class is owned by the `WebContents` and its lifetime is bound to
// lifetime of the `WebContents`.
class CONTENT_EXPORT SharedStorageBudgetCharger
    : public WebContentsObserver,
      public WebContentsUserData<SharedStorageBudgetCharger> {
 public:
  explicit SharedStorageBudgetCharger(WebContents* web_contents);
  SharedStorageBudgetCharger(const SharedStorageBudgetCharger& other) = delete;
  SharedStorageBudgetCharger& operator=(
      const SharedStorageBudgetCharger& other) = delete;
  SharedStorageBudgetCharger(SharedStorageBudgetCharger&& other) = delete;
  SharedStorageBudgetCharger& operator=(SharedStorageBudgetCharger&& other) =
      delete;
  ~SharedStorageBudgetCharger() override;

 private:
  friend class WebContentsUserData<SharedStorageBudgetCharger>;

  // WebContentsObserver:
  void DidStartNavigation(NavigationHandle* navigation_handle) override;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_STORAGE_SHARED_STORAGE_BUDGET_CHARGER_H_
