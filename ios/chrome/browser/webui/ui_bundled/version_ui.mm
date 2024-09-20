// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/version_ui.h"

#import <memory>

#import "base/command_line.h"
#import "base/i18n/time_formatting.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/stringprintf.h"
#import "base/time/time.h"
#import "build/build_config.h"
#import "components/grit/components_scaled_resources.h"
#import "components/grit/version_ui_resources.h"
#import "components/grit/version_ui_resources_map.h"
#import "components/strings/grit/components_branded_strings.h"
#import "components/strings/grit/components_strings.h"
#import "components/variations/service/variations_service.h"
#import "components/version_info/version_info.h"
#import "components/version_ui/version_handler_helper.h"
#import "components/version_ui/version_ui_constants.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/webui/ui_bundled/version_handler.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/webui/web_ui_ios.h"
#import "ios/web/public/webui/web_ui_ios_data_source.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

web::WebUIIOSDataSource* CreateVersionUIDataSource() {
  web::WebUIIOSDataSource* html_source =
      web::WebUIIOSDataSource::Create(kChromeUIVersionHost);

  // Localized and data strings.
  html_source->AddLocalizedString(version_ui::kTitle, IDS_VERSION_UI_TITLE);
  html_source->AddLocalizedString(version_ui::kLogoAltText,
                                  IDS_SHORT_PRODUCT_LOGO_ALT_TEXT);
  html_source->AddLocalizedString(version_ui::kApplicationLabel,
                                  IDS_IOS_PRODUCT_NAME);
  html_source->AddString(version_ui::kVersion,
                         std::string(version_info::GetVersionNumber()));
  html_source->AddString(version_ui::kVersionModifier,
                         std::string(GetChannelString(GetChannel())));
  html_source->AddLocalizedString(version_ui::kOSName, IDS_VERSION_UI_OS);
  html_source->AddString(version_ui::kOSType,
                         std::string(version_info::GetOSType()));

  html_source->AddLocalizedString(version_ui::kCompany,
                                  IDS_IOS_ABOUT_VERSION_COMPANY_NAME);
  html_source->AddLocalizedString(version_ui::kCopyLabel,
                                  IDS_VERSION_UI_COPY_LABEL);
  html_source->AddLocalizedString(version_ui::kCopyNotice,
                                  IDS_VERSION_UI_COPY_NOTICE);
  html_source->AddString(
      version_ui::kCopyright,
      l10n_util::GetStringFUTF16(
          IDS_IOS_ABOUT_VERSION_COPYRIGHT,
          base::LocalizedTimeFormatWithPattern(base::Time::Now(), "y")));
  html_source->AddLocalizedString(version_ui::kRevision,
                                  IDS_VERSION_UI_REVISION);
  std::string last_change(version_info::GetLastChange());
  // Shorten the git hash to display it correctly on small devices.
  if ((ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET) &&
      last_change.length() > 12) {
    last_change =
        base::StringPrintf("%s...", last_change.substr(0, 12).c_str());
  }
  html_source->AddString(version_ui::kCL, last_change);
  html_source->AddLocalizedString(version_ui::kOfficial,
                                  version_info::IsOfficialBuild()
                                      ? IDS_VERSION_UI_OFFICIAL
                                      : IDS_VERSION_UI_UNOFFICIAL);
  html_source->AddLocalizedString(
      version_ui::kVersionProcessorVariation,
      sizeof(void*) == 8 ? IDS_VERSION_UI_64BIT : IDS_VERSION_UI_32BIT);
  html_source->AddLocalizedString(version_ui::kUserAgentName,
                                  IDS_VERSION_UI_USER_AGENT);
  html_source->AddString(
      version_ui::kUserAgent,
      web::GetWebClient()->GetUserAgent(web::UserAgentType::MOBILE));
  html_source->AddLocalizedString(version_ui::kCommandLineName,
                                  IDS_VERSION_UI_COMMAND_LINE);

  std::string command_line;
  typedef std::vector<std::string> ArgvList;
  const ArgvList& argv = base::CommandLine::ForCurrentProcess()->argv();
  for (ArgvList::const_iterator iter = argv.begin(); iter != argv.end(); iter++)
    command_line += " " + *iter;
  // This assumes that `command_line` uses UTF-8 encoding.
  html_source->AddString(version_ui::kCommandLine, command_line);

  html_source->AddLocalizedString(version_ui::kVariationsName,
                                  IDS_VERSION_UI_VARIATIONS);
  html_source->AddLocalizedString(version_ui::kVariationsCmdName,
                                  IDS_VERSION_UI_VARIATIONS_CMD);
  html_source->AddLocalizedString(version_ui::kVariationsSeedName,
                                  IDS_VERSION_UI_VARIATIONS_SEED_NAME);

  html_source->AddString(
      version_ui::kVariationsSeed,
      version_ui::SeedTypeToUiString(
          GetApplicationContext()->GetVariationsService()->GetSeedType()));

  html_source->AddString(version_ui::kSanitizer,
                         std::string(version_info::GetSanitizerList()));

  html_source->UseStringsJs();
  html_source->AddResourcePaths(
      base::make_span(kVersionUiResources, kVersionUiResourcesSize));
  html_source->AddResourcePath("images/product_logo.png", IDR_PRODUCT_LOGO);
  html_source->AddResourcePath("images/product_logo_white.png",
                               IDR_PRODUCT_LOGO_WHITE);
  html_source->SetDefaultResource(IDR_VERSION_UI_ABOUT_VERSION_HTML);
  return html_source;
}

}  // namespace

VersionUI::VersionUI(web::WebUIIOS* web_ui, const std::string& host)
    : web::WebUIIOSController(web_ui, host) {
  web_ui->AddMessageHandler(std::make_unique<VersionHandler>());
  web::WebUIIOSDataSource::Add(ProfileIOS::FromWebUIIOS(web_ui),
                               CreateVersionUIDataSource());
}

VersionUI::~VersionUI() {}
