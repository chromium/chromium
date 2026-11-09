// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/model/mock_ios_chrome_aim_eligibility_service.h"

#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "third_party/omnibox_proto/input_type.pb.h"
#import "third_party/omnibox_proto/input_type_config.pb.h"
#import "third_party/omnibox_proto/input_type_rule.pb.h"
#import "third_party/omnibox_proto/rule_set.pb.h"
#import "third_party/omnibox_proto/searchbox_config.pb.h"
#import "third_party/omnibox_proto/tool_config.pb.h"
#import "third_party/omnibox_proto/tool_mode.pb.h"
#import "third_party/omnibox_proto/tool_rule.pb.h"
#import "third_party/omnibox_proto/url_param.pb.h"

namespace {
// Default maximum number of total inputs allowed in the mock searchbox config.
constexpr int kDefaultMaxTotalInputs = 10;
// Default strings and URL parameters populated in mock `ToolConfig` entries.
constexpr char kEnabledParamValue[] = "1";
constexpr char kCreateImagesLabel[] = "Create images";
constexpr char kCreateImagesHint[] = "Describe your image";
constexpr char kCreateImagesParamKey[] = "imgn";
constexpr char kCanvasLabel[] = "Canvas";
constexpr char kCanvasHint[] = "Create anything";
constexpr char kCanvasParamKey[] = "rc";
constexpr char kDeepSearchLabel[] = "Deep Search";
constexpr char kDeepSearchHint[] = "Research anything";
constexpr char kDeepSearchParamKey[] = "dr";
constexpr char kDefaultToolLabel[] = "Tool";

// Populates `tool_config` with default label, hint, URL parameter, and rule
// metadata for `tool`.
void PopulateDefaultToolConfig(omnibox::ToolMode tool,
                               omnibox::ToolConfig* tool_config) {
  tool_config->set_tool(tool);
  omnibox::ToolRule* rule = tool_config->mutable_rule();
  rule->set_tool(tool);
  rule->set_allow_all_models(true);

  switch (tool) {
    case omnibox::ToolMode::TOOL_MODE_IMAGE_GEN: {
      tool_config->set_menu_label(kCreateImagesLabel);
      tool_config->set_chip_label(kCreateImagesLabel);
      tool_config->set_hint_text(kCreateImagesHint);
      omnibox::UrlParam* url_param = tool_config->add_aim_url_params();
      url_param->set_param_key(kCreateImagesParamKey);
      url_param->set_param_value(kEnabledParamValue);
      rule->add_allowed_input_types(omnibox::InputType::INPUT_TYPE_LENS_IMAGE);
      break;
    }
    case omnibox::ToolMode::TOOL_MODE_CANVAS: {
      tool_config->set_menu_label(kCanvasLabel);
      tool_config->set_chip_label(kCanvasLabel);
      tool_config->set_hint_text(kCanvasHint);
      omnibox::UrlParam* url_param = tool_config->add_aim_url_params();
      url_param->set_param_key(kCanvasParamKey);
      url_param->set_param_value(kEnabledParamValue);
      rule->add_allowed_input_types(omnibox::InputType::INPUT_TYPE_LENS_IMAGE);
      rule->add_allowed_input_types(omnibox::InputType::INPUT_TYPE_LENS_FILE);
      break;
    }
    case omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH: {
      tool_config->set_menu_label(kDeepSearchLabel);
      tool_config->set_chip_label(kDeepSearchLabel);
      tool_config->set_hint_text(kDeepSearchHint);
      tool_config->set_disable_active_model_selection(true);
      omnibox::UrlParam* url_param = tool_config->add_aim_url_params();
      url_param->set_param_key(kDeepSearchParamKey);
      url_param->set_param_value(kEnabledParamValue);
      break;
    }
    default:
      tool_config->set_menu_label(kDefaultToolLabel);
      tool_config->set_chip_label(kDefaultToolLabel);
      rule->set_allow_all_input_types(true);
      break;
  }
}
}  // namespace

MockIOSChromeAimEligibilityService::MockIOSChromeAimEligibilityService(
    PrefService& pref_service,
    TemplateURLService* template_url_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    Configuration configuration)
    : MockAimEligibilityService(pref_service,
                                template_url_service,
                                url_loader_factory,
                                identity_manager,
                                std::move(configuration)) {
  ON_CALL(*this, IsAimEligible).WillByDefault(testing::Return(true));
  ON_CALL(*this, IsAimLocallyEligible).WillByDefault(testing::Return(true));
  ON_CALL(*this, IsServerEligibilityEnabled)
      .WillByDefault(testing::Return(false));
  // Use a mock-dedicated callback list rather than forwarding to the base
  // `AimEligibilityService` list so mock tool updates notify subscribers
  // without exposing unintended base `TemplateURLService` notifications.
  ON_CALL(*this, RegisterEligibilityChangedCallback)
      .WillByDefault([this](base::RepeatingClosure callback) {
        return mock_eligibility_changed_callbacks_.Add(std::move(callback));
      });
  config().mutable_rule_set()->set_max_total_inputs(kDefaultMaxTotalInputs);
  for (omnibox::InputType input_type :
       {omnibox::InputType::INPUT_TYPE_LENS_IMAGE,
        omnibox::InputType::INPUT_TYPE_LENS_FILE,
        omnibox::InputType::INPUT_TYPE_BROWSER_TAB}) {
    config().add_input_type_configs()->set_input_type(input_type);
    omnibox::InputTypeRule* rule =
        config().mutable_rule_set()->add_input_type_rules();
    rule->set_input_type(input_type);
    rule->set_max_instance(kDefaultMaxTotalInputs);
    rule->set_allow_all_input_types(true);
  }
  ResetDefaultTools();
}

MockIOSChromeAimEligibilityService::~MockIOSChromeAimEligibilityService() =
    default;

// static
std::unique_ptr<MockIOSChromeAimEligibilityService>
MockIOSChromeAimEligibilityService::CreateTestingProfileService(
    ProfileIOS* profile) {
  Configuration config;
  config.is_off_the_record = profile->IsOffTheRecord();
  return std::make_unique<MockIOSChromeAimEligibilityService>(
      *profile->GetPrefs(),
      ios::TemplateURLServiceFactory::GetForProfile(profile),
      profile->GetSharedURLLoaderFactory(),
      IdentityManagerFactory::GetForProfile(profile), std::move(config));
}

void MockIOSChromeAimEligibilityService::SetToolEnabled(omnibox::ToolMode tool,
                                                        bool enabled) {
  switch (tool) {
    case omnibox::ToolMode::TOOL_MODE_IMAGE_GEN:
      ON_CALL(*this, IsCreateImagesEligible)
          .WillByDefault(testing::Return(enabled));
      break;
    case omnibox::ToolMode::TOOL_MODE_CANVAS:
      ON_CALL(*this, IsCanvasEligible).WillByDefault(testing::Return(enabled));
      break;
    case omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH:
      ON_CALL(*this, IsDeepSearchEligible)
          .WillByDefault(testing::Return(enabled));
      break;
    default:
      break;
  }

  google::protobuf::RepeatedPtrField<omnibox::ToolConfig>* tool_configs =
      config().mutable_tool_configs();
  for (auto it = tool_configs->begin(); it != tool_configs->end();) {
    if (it->tool() == tool) {
      it = tool_configs->erase(it);
    } else {
      ++it;
    }
  }

  if (enabled) {
    PopulateDefaultToolConfig(tool, config().add_tool_configs());
  }

  mock_eligibility_changed_callbacks_.Notify();
}

void MockIOSChromeAimEligibilityService::ResetDefaultTools() {
  ON_CALL(*this, IsCreateImagesEligible).WillByDefault(testing::Return(true));
  ON_CALL(*this, IsCanvasEligible).WillByDefault(testing::Return(true));
  ON_CALL(*this, IsDeepSearchEligible).WillByDefault(testing::Return(true));

  config().clear_tool_configs();
  for (omnibox::ToolMode tool : {omnibox::ToolMode::TOOL_MODE_IMAGE_GEN,
                                 omnibox::ToolMode::TOOL_MODE_CANVAS,
                                 omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH}) {
    PopulateDefaultToolConfig(tool, config().add_tool_configs());
  }

  mock_eligibility_changed_callbacks_.Notify();
}
