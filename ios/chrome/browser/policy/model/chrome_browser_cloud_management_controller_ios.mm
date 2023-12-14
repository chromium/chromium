// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/chrome_browser_cloud_management_controller_ios.h"

#import <utility>

#import "base/functional/bind.h"
#import "base/task/single_thread_task_runner.h"
#import "base/task/task_traits.h"
#import "components/enterprise/browser/reporting/report_generator.h"
#import "components/enterprise/browser/reporting/report_scheduler.h"
#import "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#import "components/policy/core/common/features.h"
#import "ios/chrome/browser/shared/model/paths/paths.h"
#import "ios/chrome/browser/policy/model/browser_dm_token_storage_ios.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/model/client_data_delegate_ios.h"
#import "ios/chrome/browser/policy/model/reporting/reporting_delegate_factory_ios.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace policy {

ChromeBrowserCloudManagementControllerIOS::
    ChromeBrowserCloudManagementControllerIOS() = default;
ChromeBrowserCloudManagementControllerIOS::
    ~ChromeBrowserCloudManagementControllerIOS() = default;

void ChromeBrowserCloudManagementControllerIOS::SetDMTokenStorageDelegate() {
  BrowserDMTokenStorage::SetDelegate(
      std::make_unique<BrowserDMTokenStorageIOS>());
}

int ChromeBrowserCloudManagementControllerIOS::GetUserDataDirKey() {
  return ios::DIR_USER_DATA;
}

base::FilePath
ChromeBrowserCloudManagementControllerIOS::GetExternalPolicyDir() {
  // External policies are not supported on iOS.
  return base::FilePath();
}

ChromeBrowserCloudManagementController::Delegate::NetworkConnectionTrackerGetter
ChromeBrowserCloudManagementControllerIOS::
    CreateNetworkConnectionTrackerGetter() {
  return base::BindRepeating(&ApplicationContext::GetNetworkConnectionTracker,
                             base::Unretained(GetApplicationContext()));
}

void ChromeBrowserCloudManagementControllerIOS::InitializeOAuthTokenFactory(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService* local_state) {
  // Policy invalidations aren't currently supported on iOS.
}

void ChromeBrowserCloudManagementControllerIOS::StartWatchingRegistration(
    ChromeBrowserCloudManagementController* controller) {
  // Enrollment isn't blocking or mandatory on iOS.
}

bool ChromeBrowserCloudManagementControllerIOS::
    WaitUntilPolicyEnrollmentFinished() {
  // Enrollment currently isn't blocking or mandatory on iOS, so this method
  // isn't used. Always report success.
  return true;
}

bool ChromeBrowserCloudManagementControllerIOS::
    IsEnterpriseStartupDialogShowing() {
  // There is no enterprise startup dialog on iOS.
  return false;
}

void ChromeBrowserCloudManagementControllerIOS::OnServiceAccountSet(
    CloudPolicyClient* client,
    const std::string& account_email) {
  // Policy invalidations aren't currently supported on iOS.
}

void ChromeBrowserCloudManagementControllerIOS::ShutDown() {
  // No additional shutdown to perform on iOS.
}

MachineLevelUserCloudPolicyManager* ChromeBrowserCloudManagementControllerIOS::
    GetMachineLevelUserCloudPolicyManager() {
  return GetApplicationContext()
      ->GetBrowserPolicyConnector()
      ->machine_level_user_cloud_policy_manager();
}

DeviceManagementService*
ChromeBrowserCloudManagementControllerIOS::GetDeviceManagementService() {
  return GetApplicationContext()
      ->GetBrowserPolicyConnector()
      ->device_management_service();
}

scoped_refptr<network::SharedURLLoaderFactory>
ChromeBrowserCloudManagementControllerIOS::GetSharedURLLoaderFactory() {
  return GetApplicationContext()->GetSharedURLLoaderFactory();
}

scoped_refptr<base::SingleThreadTaskRunner>
ChromeBrowserCloudManagementControllerIOS::GetBestEffortTaskRunner() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  return web::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT});
}

std::unique_ptr<enterprise_reporting::ReportingDelegateFactory>
ChromeBrowserCloudManagementControllerIOS::GetReportingDelegateFactory() {
  return std::make_unique<enterprise_reporting::ReportingDelegateFactoryIOS>();
}

void ChromeBrowserCloudManagementControllerIOS::SetGaiaURLLoaderFactory(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  // Policy invalidations aren't currently supported on iOS.
}

bool ChromeBrowserCloudManagementControllerIOS::ReadyToCreatePolicyManager() {
  return true;
}

bool ChromeBrowserCloudManagementControllerIOS::ReadyToInit() {
  return true;
}

std::unique_ptr<ClientDataDelegate>
ChromeBrowserCloudManagementControllerIOS::CreateClientDataDelegate() {
  return std::make_unique<ClientDataDelegateIos>();
}

}  // namespace policy
