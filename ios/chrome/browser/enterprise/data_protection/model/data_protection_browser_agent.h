// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_MODEL_DATA_PROTECTION_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_MODEL_DATA_PROTECTION_BROWSER_AGENT_H_

#import "base/scoped_multi_source_observation.h"
#import "ios/chrome/browser/enterprise/data_protection/model/data_protection_tab_helper.h"
#import "ios/chrome/browser/enterprise/data_protection/model/data_protection_tab_helper_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/tabs/model/tabs_dependency_installer.h"

// Browser agent responsible for synchronizing enterprise data protection
// watermark state with profile preferences.
//
// Maintains the `kDataProtectionWatermarkedTabs` preference dictionary so that
// UI surfaces (such as the Tab Grid) can render watermarks accordingly.
class DataProtectionBrowserAgent
    : public BrowserUserData<DataProtectionBrowserAgent>,
      public TabsDependencyInstaller,
      public DataProtectionTabHelperObserver {
 public:
  DataProtectionBrowserAgent(const DataProtectionBrowserAgent&) = delete;
  DataProtectionBrowserAgent& operator=(const DataProtectionBrowserAgent&) =
      delete;

  ~DataProtectionBrowserAgent() override;

 private:
  friend class BrowserUserData<DataProtectionBrowserAgent>;
  friend class DataProtectionBrowserAgentTest;

  explicit DataProtectionBrowserAgent(Browser* browser);

  // TabsDependencyInstaller overrides:
  void OnWebStateInserted(web::WebState* web_state) override;
  void OnWebStateRemoved(web::WebState* web_state) override;
  void OnWebStateDeleted(web::WebState* web_state) override;
  void OnActiveWebStateChanged(web::WebState* old_active,
                               web::WebState* new_active) override;

  // DataProtectionTabHelperObserver overrides:
  void WatermarkTextDidChange(web::WebState* web_state,
                              const std::string& watermark_text) override;

  // Scoped observations of DataProtectionTabHelper instances for all realized
  // tabs in the browser.
  base::ScopedMultiSourceObservation<DataProtectionTabHelper,
                                     DataProtectionTabHelperObserver>
      observers_{this};
};

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_MODEL_DATA_PROTECTION_BROWSER_AGENT_H_
