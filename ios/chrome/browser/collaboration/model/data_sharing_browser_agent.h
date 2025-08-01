// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COLLABORATION_MODEL_DATA_SHARING_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_COLLABORATION_MODEL_DATA_SHARING_BROWSER_AGENT_H_

#include "base/memory/raw_ptr.h"
#include "ios/chrome/browser/collaboration/model/data_sharing_tab_helper_delegate.h"
#include "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#include "ios/chrome/browser/tabs/model/tabs_dependency_installer.h"

namespace collaboration {
class CollaborationService;
}  // namespace collaboration

// Implements DataSharingTabHelperDelegate for the DataSharingTabHelper
// inserted in the associated Browser.
class DataSharingBrowserAgent : public BrowserUserData<DataSharingBrowserAgent>,
                                public DataSharingTabHelperDelegate,
                                public TabsDependencyInstaller {
 public:
  ~DataSharingBrowserAgent() override;

  // DataSharingTabHelperDelegate implementation.
  bool IsAllowedToJoinSharedTabGroups() override;
  void HandleShareURLNavigationIntercepted(
      const GURL& url,
      collaboration::CollaborationServiceJoinEntryPoint entry) override;

  // TabsDependencyInstaller implementation.
  void OnWebStateInserted(web::WebState* web_state) override;
  void OnWebStateRemoved(web::WebState* web_state) override;
  void OnWebStateDeleted(web::WebState* web_state) override;
  void OnActiveWebStateChanged(web::WebState* old_active,
                               web::WebState* new_active) override;

 private:
  friend class BrowserUserData<DataSharingBrowserAgent>;
  DataSharingBrowserAgent(Browser* browser,
                          collaboration::CollaborationService* service);

  raw_ptr<collaboration::CollaborationService> service_;
};

#endif  // IOS_CHROME_BROWSER_COLLABORATION_MODEL_DATA_SHARING_BROWSER_AGENT_H_
