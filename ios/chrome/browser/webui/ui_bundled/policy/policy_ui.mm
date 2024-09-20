// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/policy/policy_ui.h"

#import <memory>
#import <string>

#import "base/json/json_string_value_serializer.h"
#import "base/json/json_writer.h"
#import "components/grit/policy_resources.h"
#import "components/grit/policy_resources_map.h"
#import "components/policy/core/browser/policy_conversions.h"
#import "components/policy/core/common/policy_loader_common.h"
#import "components/policy/core/common/policy_logger.h"
#import "components/policy/core/common/policy_utils.h"
#import "components/policy/core/common/schema.h"
#import "components/policy/policy_constants.h"
#import "components/strings/grit/components_branded_strings.h"
#import "components/strings/grit/components_strings.h"
#import "components/version_info/version_info.h"
#import "components/version_ui/version_handler_helper.h"
#import "ios/chrome/browser/policy/model/browser_state_policy_connector.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/webui/ui_bundled/policy/policy_ui_handler.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/web/public/webui/web_ui_ios.h"
#import "ios/web/public/webui/web_ui_ios_data_source.h"
#import "ios/web/public/webui/web_ui_ios_message_handler.h"
#import "ui/base/webui/web_ui_util.h"

namespace {

base::Value::List GetChromePolicyNames(ProfileIOS* profile) {
  policy::SchemaRegistry* registry =
      profile->GetPolicyConnector()->GetSchemaRegistry();
  scoped_refptr<policy::SchemaMap> schema_map = registry->schema_map();

  // Add Chrome policy names.
  base::Value::List chrome_policy_names;
  policy::PolicyNamespace chrome_namespace(policy::POLICY_DOMAIN_CHROME, "");
  const policy::Schema* chrome_schema = schema_map->GetSchema(chrome_namespace);
  for (auto it = chrome_schema->GetPropertiesIterator(); !it.IsAtEnd();
       it.Advance()) {
    chrome_policy_names.Append(base::Value(it.key()));
  }

  chrome_policy_names.EraseIf([&](auto& policy) {
    return policy::IsPolicyNameSensitive(policy.GetString());
  });
  return chrome_policy_names;
}

// Returns the version information to be displayed on the chrome://policy/logs
// page.
base::Value::Dict GetVersionInfo() {
  base::Value::Dict version_info;

  version_info.Set("revision", version_info::GetLastChange());
  version_info.Set("version", version_info::GetVersionNumber());
  version_info.Set("deviceOs", "iOS");
  version_info.Set("variations", version_ui::GetVariationsList());

  return version_info;
}

web::WebUIIOSDataSource* CreatePolicyUIHtmlSource(ProfileIOS* profile) {
  web::WebUIIOSDataSource* source =
      web::WebUIIOSDataSource::Create(kChromeUIPolicyHost);
  PolicyUIHandler::AddCommonLocalizedStringsToSource(source);

  static constexpr webui::LocalizedString kStrings[] = {
      // Localized strings (alphabetical order).
      {"copyPoliciesJSON", IDS_COPY_POLICIES_JSON},
      {"exportPoliciesJSON", IDS_EXPORT_POLICIES_JSON},
      {"filterPlaceholder", IDS_POLICY_FILTER_PLACEHOLDER},
      {"hideExpandedStatus", IDS_POLICY_HIDE_EXPANDED_STATUS},
      {"isAffiliatedYes", IDS_POLICY_IS_AFFILIATED_YES},
      {"isAffiliatedNo", IDS_POLICY_IS_AFFILIATED_NO},
      {"labelAssetId", IDS_POLICY_LABEL_ASSET_ID},
      {"labelClientId", IDS_POLICY_LABEL_CLIENT_ID},
      {"labelDirectoryApiId", IDS_POLICY_LABEL_DIRECTORY_API_ID},
      {"labelError", IDS_POLICY_LABEL_ERROR},
      {"labelWarning", IDS_POLICY_HEADER_WARNING},
      {"labelGaiaId", IDS_POLICY_LABEL_GAIA_ID},
      {"labelIsAffiliated", IDS_POLICY_LABEL_IS_AFFILIATED},
      {"labelLastCloudReportSentTimestamp",
       IDS_POLICY_LABEL_LAST_CLOUD_REPORT_SENT_TIMESTAMP},
      {"labelLocation", IDS_POLICY_LABEL_LOCATION},
      {"labelMachineEnrollmentDomain",
       IDS_POLICY_LABEL_MACHINE_ENROLLMENT_DOMAIN},
      {"labelMachineEnrollmentMachineName",
       IDS_POLICY_LABEL_MACHINE_ENROLLMENT_MACHINE_NAME},
      {"labelMachineEnrollmentToken",
       IDS_POLICY_LABEL_MACHINE_ENROLLMENT_TOKEN},
      {"labelMachineEntrollmentDeviceId",
       IDS_POLICY_LABEL_MACHINE_ENROLLMENT_DEVICE_ID},
      {"labelIsOffHoursActive", IDS_POLICY_LABEL_IS_OFFHOURS_ACTIVE},
      {"labelPoliciesPush", IDS_POLICY_LABEL_PUSH_POLICIES},
      {"labelPrecedence", IDS_POLICY_LABEL_PRECEDENCE},
      {"labelProfileId", IDS_POLICY_LABEL_PROFILE_ID},
      {"labelRefreshInterval", IDS_POLICY_LABEL_REFRESH_INTERVAL},
      {"labelStatus", IDS_POLICY_LABEL_STATUS},
      {"labelTimeSinceLastFetchAttempt",
       IDS_POLICY_LABEL_TIME_SINCE_LAST_FETCH_ATTEMPT},
      {"labelTimeSinceLastRefresh", IDS_POLICY_LABEL_TIME_SINCE_LAST_REFRESH},
      {"labelUsername", IDS_POLICY_LABEL_USERNAME},
      {"labelManagedBy", IDS_POLICY_LABEL_MANAGED_BY},
      {"labelVersion", IDS_POLICY_LABEL_VERSION},
      {"moreActions", IDS_POLICY_MORE_ACTIONS},
      {"noPoliciesSet", IDS_POLICY_NO_POLICIES_SET},
      {"offHoursActive", IDS_POLICY_OFFHOURS_ACTIVE},
      {"offHoursNotActive", IDS_POLICY_OFFHOURS_NOT_ACTIVE},
      {"policiesPushOff", IDS_POLICY_PUSH_POLICIES_OFF},
      {"policiesPushOn", IDS_POLICY_PUSH_POLICIES_ON},
      {"policyCopyValue", IDS_POLICY_COPY_VALUE},
      {"policyLearnMore", IDS_POLICY_LEARN_MORE},
      {"reloadPolicies", IDS_POLICY_RELOAD_POLICIES},
      {"showExpandedStatus", IDS_POLICY_SHOW_EXPANDED_STATUS},
      {"showLess", IDS_POLICY_SHOW_LESS},
      {"showMore", IDS_POLICY_SHOW_MORE},
      {"showUnset", IDS_POLICY_SHOW_UNSET},
      {"signinProfile", IDS_POLICY_SIGNIN_PROFILE},
      {"status", IDS_POLICY_STATUS},
      {"statusErrorManagedNoPolicy", IDS_POLICY_STATUS_ERROR_MANAGED_NO_POLICY},
      {"statusFlexOrgNoPolicy", IDS_POLICY_STATUS_FLEX_ORG_NO_POLICY},
      {"statusDevice", IDS_POLICY_STATUS_DEVICE},
      {"statusMachine", IDS_POLICY_STATUS_MACHINE},
      {"statusUser", IDS_POLICY_STATUS_USER},
      {"uploadReport", IDS_UPLOAD_REPORT},
      {"viewLogs", IDS_VIEW_POLICY_LOGS},
  };
  source->AddLocalizedStrings(kStrings);

  const bool allow_policy_test_page = PolicyUI::ShouldLoadTestPage(profile);

  // Test page should only load if testing is enabled.
  if (allow_policy_test_page) {
    // Localized strings for chrome://policy/test.
    static constexpr webui::LocalizedString kPolicyTestStrings[] = {
        {"testTitle", IDS_POLICY_TEST_TITLE},
        {"testRestart", IDS_POLICY_TEST_RESTART_AND_APPLY},
        {"testApply", IDS_POLICY_TEST_APPLY},
        {"testImport", IDS_POLICY_TEST_IMPORT},
        {"testDesc", IDS_POLICY_TEST_DESC},
        {"testRevertAppliedPolicies", IDS_POLICY_TEST_REVERT},
        {"testClearPolicies", IDS_CLEAR},
        {"testTableName", IDS_POLICY_HEADER_NAME},
        {"testTableSource", IDS_POLICY_HEADER_SOURCE},
        {"testTableScope", IDS_POLICY_TEST_TABLE_SCOPE},
        {"testTableLevel", IDS_POLICY_HEADER_LEVEL},
        {"testTableValue", IDS_POLICY_LABEL_VALUE},
        {"testTableRemove", IDS_REMOVE},
        {"testAdd", IDS_POLICY_TEST_ADD},
        {"testNameSelect", IDS_POLICY_SELECT_NAME},
        {"testTableNamespace", IDS_POLICY_HEADER_NAMESPACE},
        {"testTablePreset", IDS_POLICY_TEST_TABLE_PRESET},
        {"testTablePresetCustom", IDS_POLICY_TEST_PRESET_CUSTOM},
        {"testTablePresetLocalMachine", IDS_POLICY_TEST_PRESET_LOCAL_MACHINE},
        {"testTablePresetCloudAccount", IDS_POLICY_TEST_PRESET_CLOUD_ACCOUNT},
        {"testUserAffiliated", IDS_POLICY_TEST_USER_AFFILIATED},
    };

    source->AddLocalizedStrings(kPolicyTestStrings);
    source->AddResourcePath("test/policy_test.js",
                            IDR_POLICY_TEST_POLICY_TEST_JS);
    source->AddResourcePath("test/", IDR_POLICY_TEST_POLICY_TEST_HTML);
    source->AddResourcePath("test", IDR_POLICY_TEST_POLICY_TEST_HTML);

    // Create a string policy_names_to_types mapping policy names to their
    // input types.
    policy::Schema chrome_schema =
        policy::Schema::Wrap(policy::GetChromeSchemaData());
    base::Value::List policy_names = GetChromePolicyNames(profile);

    std::string schema;
    JSONStringValueSerializer serializer(&schema);
    serializer.Serialize(PolicyUI::GetSchema(profile));
    source->AddString("initialSchema", schema);

    // Strings for policy levels, scopes and sources.
    static constexpr webui::LocalizedString kPolicyTestTypes[] = {
        {"scopeUser", IDS_POLICY_SCOPE_USER},
        {"scopeDevice", IDS_POLICY_SCOPE_DEVICE},
        {"levelRecommended", IDS_POLICY_LEVEL_RECOMMENDED},
        {"levelMandatory", IDS_POLICY_LEVEL_MANDATORY},
        {"sourceEnterpriseDefault", IDS_POLICY_SOURCE_ENTERPRISE_DEFAULT},
        {"sourceCommandLine", IDS_POLICY_SOURCE_COMMAND_LINE},
        {"sourceCloud", IDS_POLICY_SOURCE_CLOUD},
        {"sourceActiveDirectory", IDS_POLICY_SOURCE_ACTIVE_DIRECTORY},
        {"sourcePlatform", IDS_POLICY_SOURCE_PLATFORM},
        {"sourceMerged", IDS_POLICY_SOURCE_MERGED},
        {"sourceCloudFromAsh", IDS_POLICY_SOURCE_CLOUD_FROM_ASH},
        {"sourceRestrictedManagedGuestSessionOverride",
         IDS_POLICY_SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE},
    };

    source->AddLocalizedStrings(kPolicyTestTypes);
  }

  source->AddString("acceptedPaths",
                    allow_policy_test_page ? "/|/test|/logs" : "/|/logs");
  // Localized strings for chrome://policy/logs.
  static constexpr webui::LocalizedString kPolicyLogsStrings[] = {
      {"browserName", IDS_IOS_PRODUCT_NAME},
      {"exportLogsJSON", IDS_EXPORT_POLICY_LOGS_JSON},
      {"logsTitle", IDS_POLICY_LOGS_TITLE},
      {"os", IDS_VERSION_UI_OS},
      {"refreshLogs", IDS_REFRESH_POLICY_LOGS},
      {"revision", IDS_VERSION_UI_REVISION},
      {"versionInfoLabel", IDS_VERSION_INFO},
      {"variations", IDS_VERSION_UI_VARIATIONS},
  };
  source->AddLocalizedStrings(kPolicyLogsStrings);

  source->UseStringsJs();

  source->AddBoolean("hideExportButton", true);

  source->AddResourcePaths(
      base::make_span(kPolicyResources, kPolicyResourcesSize));

  std::string variations_json_value;
  base::JSONWriter::Write(GetVersionInfo(), &variations_json_value);
  source->AddString("versionInfo", variations_json_value);

  source->AddResourcePath("logs/policy_logs.js",
                          IDR_POLICY_LOGS_POLICY_LOGS_JS);
  source->AddResourcePath("logs/", IDR_POLICY_LOGS_POLICY_LOGS_HTML);
  source->AddResourcePath("logs", IDR_POLICY_LOGS_POLICY_LOGS_HTML);

  source->SetDefaultResource(IDR_POLICY_POLICY_HTML);
  source->EnableReplaceI18nInJS();
  return source;
}

}  // namespace

PolicyUI::PolicyUI(web::WebUIIOS* web_ui, const std::string& host)
    : web::WebUIIOSController(web_ui, host) {
  web_ui->AddMessageHandler(std::make_unique<PolicyUIHandler>());
  ProfileIOS* profile = ProfileIOS::FromWebUIIOS(web_ui);
  web::WebUIIOSDataSource::Add(profile, CreatePolicyUIHtmlSource(profile));
}

// static
bool PolicyUI::ShouldLoadTestPage(ProfileIOS* profile) {
  AuthenticationService* auth_service =
      AuthenticationServiceFactory::GetForProfile(profile);
  // Test page should only load if testing is enabled and the profile is not
  // managed.
  return policy::utils::IsPolicyTestingEnabled(profile->GetPrefs(),
                                               GetChannel()) &&
         !auth_service->HasPrimaryIdentityManaged(
             signin::ConsentLevel::kSignin);
}

// static
base::Value PolicyUI::GetSchema(ProfileIOS* profile) {
  // Build a dictionary like this:
  // {
  //   "chrome": {
  //     "PolicyOne": "number",
  //     "PolicyTwo": "string",
  //     ...
  //   }
  // }
  // A dictionary is used to be consistent with other platforms sharing the
  // policy test page frontend implementation.
  base::Value::Dict dict;

  // Create a string policy_names_to_types mapping policy names to their
  // input types.
  policy::Schema chrome_schema =
      policy::Schema::Wrap(policy::GetChromeSchemaData());
  base::Value::List policy_names = GetChromePolicyNames(profile);

  // "chrome" is the only namespace on iOS.
  dict.Set("chrome", policy::utils::GetPolicyNameToTypeMapping(policy_names,
                                                               chrome_schema));

  return base::Value(std::move(dict));
}

PolicyUI::~PolicyUI() {}
