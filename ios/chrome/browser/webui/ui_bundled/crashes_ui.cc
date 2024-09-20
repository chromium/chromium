// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/webui/ui_bundled/crashes_ui.h"

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "components/crash/core/browser/crashes_ui_util.h"
#include "components/crash/core/common/reporter_running_ios.h"
#include "components/grit/dev_ui_components_resources.h"
#include "components/strings/grit/components_branded_strings.h"
#include "components/strings/grit/components_strings.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/browser/crash_report/model/crash_upload_list.h"
#include "ios/chrome/browser/metrics/model/ios_chrome_metrics_service_accessor.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#include "ios/chrome/grit/ios_branded_strings.h"
#include "ios/web/public/webui/web_ui_ios.h"
#include "ios/web/public/webui/web_ui_ios_data_source.h"
#include "ios/web/public/webui/web_ui_ios_message_handler.h"

namespace {

web::WebUIIOSDataSource* CreateCrashesUIHTMLSource() {
  web::WebUIIOSDataSource* source =
      web::WebUIIOSDataSource::Create(kChromeUICrashesHost);

  for (size_t i = 0; i < crash_reporter::kCrashesUILocalizedStringsCount; ++i) {
    source->AddLocalizedString(
        crash_reporter::kCrashesUILocalizedStrings[i].name,
        crash_reporter::kCrashesUILocalizedStrings[i].resource_id);
  }

  source->AddLocalizedString(crash_reporter::kCrashesUIShortProductName,
                             IDS_IOS_SHORT_PRODUCT_NAME);

  source->UseStringsJs();
  source->AddResourcePath(crash_reporter::kCrashesUICrashesJS,
                          IDR_CRASH_CRASHES_JS);
  source->AddResourcePath(crash_reporter::kCrashesUICrashesCSS,
                          IDR_CRASH_CRASHES_CSS);
  source->AddResourcePath(crash_reporter::kCrashesUISadTabSVG,
                          IDR_CRASH_SADTAB_SVG);
  source->SetDefaultResource(IDR_CRASH_CRASHES_HTML);
  return source;
}

////////////////////////////////////////////////////////////////////////////////
//
// CrashesDOMHandler
//
////////////////////////////////////////////////////////////////////////////////

// The handler for Javascript messages for the chrome://crashes/ page.
class CrashesDOMHandler : public web::WebUIIOSMessageHandler {
 public:
  CrashesDOMHandler();

  CrashesDOMHandler(const CrashesDOMHandler&) = delete;
  CrashesDOMHandler& operator=(const CrashesDOMHandler&) = delete;

  ~CrashesDOMHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;


 private:
  // Crash UploadList callback.
  void OnUploadListAvailable();

  // Asynchronously fetches the list of crashes. Called from JS.
  void HandleRequestCrashes(const base::Value::List& args);

  // Asynchronously requests a user triggered upload. Called from JS.
  void HandleRequestSingleCrashUpload(const base::Value::List& args);

  // Sends the recent crashes list JS.
  void UpdateUI();

  scoped_refptr<UploadList> upload_list_;
  bool list_available_;
  bool first_load_;
};

CrashesDOMHandler::CrashesDOMHandler()
    : list_available_(false), first_load_(true) {
  upload_list_ = ios::CreateCrashUploadList();
}

CrashesDOMHandler::~CrashesDOMHandler() {
  upload_list_->CancelLoadCallback();
}

void CrashesDOMHandler::RegisterMessages() {
  upload_list_->Load(base::BindOnce(&CrashesDOMHandler::OnUploadListAvailable,
                                    base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      crash_reporter::kCrashesUIRequestCrashList,
      base::BindRepeating(&CrashesDOMHandler::HandleRequestCrashes,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      crash_reporter::kCrashesUIRequestSingleCrashUpload,
      base::BindRepeating(&CrashesDOMHandler::HandleRequestSingleCrashUpload,
                          base::Unretained(this)));
}

void CrashesDOMHandler::HandleRequestCrashes(const base::Value::List& args) {
  if (first_load_) {
    first_load_ = false;
    if (list_available_)
      UpdateUI();
  } else {
    list_available_ = false;
    upload_list_->Load(base::BindOnce(&CrashesDOMHandler::OnUploadListAvailable,
                                      base::Unretained(this)));
  }
}

void CrashesDOMHandler::HandleRequestSingleCrashUpload(
    const base::Value::List& args) {
  DCHECK(crash_reporter::IsCrashpadRunning());
  if (!IOSChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled()) {
    return;
  }

  std::string local_id = args[0].GetString();
  upload_list_->RequestSingleUploadAsync(local_id);
}

void CrashesDOMHandler::OnUploadListAvailable() {
  list_available_ = true;
  if (!first_load_)
    UpdateUI();
}

void CrashesDOMHandler::UpdateUI() {
  bool crash_reporting_enabled =
      IOSChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled();
  base::Value::List crash_list;
  if (crash_reporting_enabled)
    crash_reporter::UploadListToValue(upload_list_.get(), &crash_list);

  base::Value::Dict result;
  result.Set("enabled", crash_reporting_enabled);
  result.Set("dynamicBackend", false);
  result.Set("manualUploads", crash_reporter::IsCrashpadRunning());
  result.Set("crashes", std::move(crash_list));
  result.Set("version", version_info::GetVersionNumber());
  result.Set("os", base::SysInfo::OperatingSystemName() + " " +
                       base::SysInfo::OperatingSystemVersion());

  base::Value event_name(crash_reporter::kCrashesUIUpdateCrashList);

  base::ValueView args[] = {event_name, result};
  web_ui()->CallJavascriptFunction("cr.webUIListenerCallback", args);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// CrashesUI
//
///////////////////////////////////////////////////////////////////////////////

CrashesUI::CrashesUI(web::WebUIIOS* web_ui, const std::string& host)
    : web::WebUIIOSController(web_ui, host) {
  web_ui->AddMessageHandler(std::make_unique<CrashesDOMHandler>());

  // Set up the chrome://crashes/ source.
  web::WebUIIOSDataSource::Add(ProfileIOS::FromWebUIIOS(web_ui),
                               CreateCrashesUIHTMLSource());
}
