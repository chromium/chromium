// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/analysis/test/analysis_connectors_app_interface.h"

#import "components/enterprise/connectors/core/analysis_test_utils.h"
#import "components/enterprise/connectors/core/common.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/test/app/chrome_test_util.h"

@implementation AnalysisConnectorsAppInterface

+ (void)setDownloadProtectionRules {
  PrefService* prefs = chrome_test_util::GetOriginalProfile()->GetPrefs();
  enterprise_connectors::test::SetAnalysisConnectorsPrefs(
      prefs, enterprise_connectors::AnalysisConnector::FILE_DOWNLOADED, {R"({
        "service_provider": "google",
        "enable": [{
          "url_list": ["*"],
          "tags": ["dlp", "malware"]
        }],
          "block_until_verdict": 1,
          "block_password_protected": true,
          "block_large_files": true
        })"},
      /*machine_scope=*/true);
}

+ (void)clearDownloadProtectionRules {
  PrefService* prefs = chrome_test_util::GetOriginalProfile()->GetPrefs();
  prefs->ClearPref(enterprise_connectors::AnalysisConnectorPref(
      enterprise_connectors::AnalysisConnector::FILE_DOWNLOADED));
}

@end
