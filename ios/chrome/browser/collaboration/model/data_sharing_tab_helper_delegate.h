// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COLLABORATION_MODEL_DATA_SHARING_TAB_HELPER_DELEGATE_H_
#define IOS_CHROME_BROWSER_COLLABORATION_MODEL_DATA_SHARING_TAB_HELPER_DELEGATE_H_

#include "components/collaboration/public/collaboration_flow_entry_point.h"
#include "url/gurl.h"

// Delegate for DataSharingTabHelper.
class DataSharingTabHelperDelegate {
 public:
  DataSharingTabHelperDelegate() = default;
  virtual ~DataSharingTabHelperDelegate() = default;

  DataSharingTabHelperDelegate(const DataSharingTabHelperDelegate&) = delete;
  DataSharingTabHelperDelegate& operator=(const DataSharingTabHelperDelegate&) =
      delete;

  // Returns true if joining shared tab groups is allowed.
  virtual bool IsAllowedToJoinSharedTabGroups() = 0;

  // Called when a data sharing URL has been intercepted.
  virtual void HandleShareURLNavigationIntercepted(
      const GURL& url,
      collaboration::CollaborationServiceJoinEntryPoint entry) = 0;
};

#endif  // IOS_CHROME_BROWSER_COLLABORATION_MODEL_DATA_SHARING_TAB_HELPER_DELEGATE_H_
