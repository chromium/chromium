// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/analysis/test/analysis_connectors_app_interface.h"

#import "components/enterprise/browser/controller/browser_dm_token_storage.h"
#import "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#import "components/enterprise/connectors/core/analysis_test_utils.h"
#import "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_service.h"
#import "components/enterprise/connectors/core/cloud_content_scanning/cloud_binary_upload_service_base.h"
#import "components/enterprise/connectors/core/common.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/ios_cloud_binary_upload_service.h"
#import "ios/chrome/browser/enterprise/cloud_content_scanning/model/ios_cloud_binary_upload_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/test/app/chrome_test_util.h"

constexpr char kFakeBrowserDMToken[] = "fake-browser-dm-token";
constexpr char kFakeBrowserClientId[] = "fake-browser-client-id";
constexpr char kFakeEnrollmentToken[] = "fake-enrollment-token";

@implementation AnalysisConnectorsAppInterface {
  std::unique_ptr<policy::FakeBrowserDMTokenStorage> _tokenStorage;
}

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

+ (void)setBrowserDMToken {
  AnalysisConnectorsAppInterface* instance =
      [AnalysisConnectorsAppInterface sharedInstance];
  instance->_tokenStorage =
      std::make_unique<policy::FakeBrowserDMTokenStorage>();
  instance->_tokenStorage->SetEnrollmentToken(kFakeEnrollmentToken);
  instance->_tokenStorage->SetClientId(kFakeBrowserClientId);
  instance->_tokenStorage->EnableStorage(true);
  instance->_tokenStorage->SetDMToken(kFakeBrowserDMToken);
  policy::BrowserDMTokenStorage::SetForTesting(instance->_tokenStorage.get());

  Browser* browser = chrome_test_util::GetCurrentBrowser();
  enterprise_connectors::BinaryUploadService* service =
      enterprise_connectors::IOSCloudBinaryUploadServiceFactory::GetForProfile(
          browser->GetProfile());

  static_cast<enterprise_connectors::CloudBinaryUploadServiceBase*>(service)
      ->SetAuthForTesting(kFakeBrowserDMToken,
                          /*auth_check_result=*/enterprise_connectors::
                              ScanRequestUploadResult::kSuccess);
}

+ (void)clearBrowserDMToken {
  AnalysisConnectorsAppInterface* instance =
      [AnalysisConnectorsAppInterface sharedInstance];
  if (!instance || !instance->_tokenStorage) {
    return;
  }
  policy::BrowserDMTokenStorage::SetForTesting(nullptr);
  instance->_tokenStorage = nullptr;
}

#pragma mark - private methods

+ (instancetype)sharedInstance {
  static AnalysisConnectorsAppInterface* sharedInstance = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    sharedInstance = [[AnalysisConnectorsAppInterface alloc] init];
  });
  return sharedInstance;
}

@end
