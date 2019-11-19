// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/webui/crashes_ui.h"

#include <stddef.h>

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "components/crash/core/browser/crashes_ui_util.h"
#include "components/grit/components_resources.h"
#include "components/strings/grit/components_chromium_strings.h"
#include "components/strings/grit/components_strings.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/crash_report/crash_upload_list.h"
#include "ios/chrome/browser/metrics/ios_chrome_metrics_service_accessor.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
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
  ~CrashesDOMHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;


 private:
  // Crash UploadList callback.
  void OnUploadListAvailable();

  // Asynchronously fetches the list of crashes. Called from JS.
  void HandleRequestCrashes(const base::ListValue* args);

  // Sends the recent crashes list JS.
  void UpdateUI();

  scoped_refptr<UploadList> upload_list_;
  bool list_available_;
  bool first_load_;

  DISALLOW_COPY_AND_ASSIGN(CrashesDOMHandler);
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
}

void CrashesDOMHandler::HandleRequestCrashes(const base::ListValue* args) {
  if (first_load_) {
    first_load_ = false;
    if (list_available_)
      UpdateUI();
  } else {
    list_available_ = false;
    upload_list_->Load(base::Bind(&CrashesDOMHandler::OnUploadListAvailable,
                                  base::Unretained(this)));
  }
}

void CrashesDOMHandler::OnUploadListAvailable() {
  list_available_ = true;
  if (!first_load_)
    UpdateUI();
}

void CrashesDOMHandler::UpdateUI() {
  bool crash_reporting_enabled =
      IOSChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled();
  base::ListValue crash_list;
  if (crash_reporting_enabled)
    crash_reporter::UploadListToValue(upload_list_.get(), &crash_list);
  base::Value enabled(crash_reporting_enabled);
  base::Value dynamic_backend(false);
  base::Value manual_uploads(false);
  base::Value version(version_info::GetVersionNumber());
  base::Value os_string(base::SysInfo::OperatingSystemName() + " " +
                        base::SysInfo::OperatingSystemVersion());

  std::vector<const base::Value*> args;
  args.push_back(&enabled);
  args.push_back(&dynamic_backend);
  args.push_back(&manual_uploads);
  args.push_back(&crash_list);
  args.push_back(&version);
  args.push_back(&os_string);
  web_ui()->CallJavascriptFunction(crash_reporter::kCrashesUIUpdateCrashList,
                                   args);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// CrashesUI
//
///////////////////////////////////////////////////////////////////////////////

CrashesUI::CrashesUI(web::WebUIIOS* web_ui) : web::WebUIIOSController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<CrashesDOMHandler>());

  // Set up the chrome://crashes/ source.
  web::WebUIIOSDataSource::Add(ios::ChromeBrowserState::FromWebUIIOS(web_ui),
                               CreateCrashesUIHTMLSource());
}
