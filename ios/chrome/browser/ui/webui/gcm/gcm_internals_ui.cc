// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/webui/gcm/gcm_internals_ui.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/gcm_driver/gcm_client.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/gcm_internals_constants.h"
#include "components/gcm_driver/gcm_internals_helper.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/grit/components_resources.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/gcm/ios_chrome_gcm_profile_service_factory.h"
#include "ios/web/public/webui/web_ui_ios.h"
#include "ios/web/public/webui/web_ui_ios_data_source.h"
#include "ios/web/public/webui/web_ui_ios_message_handler.h"

namespace {

// Class acting as a controller of the chrome://gcm-internals WebUI.
class GcmInternalsUIMessageHandler : public web::WebUIIOSMessageHandler {
 public:
  GcmInternalsUIMessageHandler();
  ~GcmInternalsUIMessageHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

 private:
  // Return all of the GCM related infos to the gcm-internals page by calling
  // Javascript callback function |gcm-internals.returnInfo()|.
  void ReturnResults(PrefService* prefs,
                     gcm::GCMProfileService* profile_service,
                     const gcm::GCMClient::GCMStatistics* stats) const;

  // Request all of the GCM related infos through gcm profile service.
  void RequestAllInfo(const base::ListValue* args);

  // Enables/disables GCM activity recording through gcm profile service.
  void SetRecording(const base::ListValue* args);

  // Callback function of the request for all gcm related infos.
  void RequestGCMStatisticsFinished(
      const gcm::GCMClient::GCMStatistics& args) const;

  // Factory for creating references in callbacks.
  base::WeakPtrFactory<GcmInternalsUIMessageHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GcmInternalsUIMessageHandler);
};

GcmInternalsUIMessageHandler::GcmInternalsUIMessageHandler()
    : weak_ptr_factory_(this) {}

GcmInternalsUIMessageHandler::~GcmInternalsUIMessageHandler() {}

void GcmInternalsUIMessageHandler::ReturnResults(
    PrefService* prefs,
    gcm::GCMProfileService* profile_service,
    const gcm::GCMClient::GCMStatistics* stats) const {
  base::DictionaryValue results;
  gcm_driver::SetGCMInternalsInfo(stats, profile_service, prefs, &results);

  std::vector<const base::Value*> args{&results};
  web_ui()->CallJavascriptFunction(gcm_driver::kSetGcmInternalsInfo, args);
}

void GcmInternalsUIMessageHandler::RequestAllInfo(const base::ListValue* args) {
  if (args->GetSize() != 1) {
    NOTREACHED();
    return;
  }
  bool clear_logs = false;
  if (!args->GetBoolean(0, &clear_logs)) {
    NOTREACHED();
    return;
  }

  gcm::GCMDriver::ClearActivityLogs clear_activity_logs =
      clear_logs ? gcm::GCMDriver::CLEAR_LOGS : gcm::GCMDriver::KEEP_LOGS;

  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromWebUIIOS(web_ui());
  gcm::GCMProfileService* profile_service =
      IOSChromeGCMProfileServiceFactory::GetForBrowserState(browser_state);

  if (!profile_service || !profile_service->driver()) {
    ReturnResults(browser_state->GetPrefs(), nullptr, nullptr);
  } else {
    profile_service->driver()->GetGCMStatistics(
        base::Bind(&GcmInternalsUIMessageHandler::RequestGCMStatisticsFinished,
                   weak_ptr_factory_.GetWeakPtr()),
        clear_activity_logs);
  }
}

void GcmInternalsUIMessageHandler::SetRecording(const base::ListValue* args) {
  if (args->GetSize() != 1) {
    NOTREACHED();
    return;
  }
  bool recording = false;
  if (!args->GetBoolean(0, &recording)) {
    NOTREACHED();
    return;
  }

  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromWebUIIOS(web_ui());
  gcm::GCMProfileService* profile_service =
      IOSChromeGCMProfileServiceFactory::GetForBrowserState(browser_state);

  if (!profile_service) {
    ReturnResults(browser_state->GetPrefs(), nullptr, nullptr);
    return;
  }
  // Get fresh stats after changing recording setting.
  profile_service->driver()->SetGCMRecording(
      base::Bind(&GcmInternalsUIMessageHandler::RequestGCMStatisticsFinished,
                 weak_ptr_factory_.GetWeakPtr()),
      recording);
}

void GcmInternalsUIMessageHandler::RequestGCMStatisticsFinished(
    const gcm::GCMClient::GCMStatistics& stats) const {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromWebUIIOS(web_ui());
  DCHECK(browser_state);
  gcm::GCMProfileService* profile_service =
      IOSChromeGCMProfileServiceFactory::GetForBrowserState(browser_state);

  DCHECK(profile_service);
  ReturnResults(browser_state->GetPrefs(), profile_service, &stats);
}

void GcmInternalsUIMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      gcm_driver::kGetGcmInternalsInfo,
      base::BindRepeating(&GcmInternalsUIMessageHandler::RequestAllInfo,
                          weak_ptr_factory_.GetWeakPtr()));
  web_ui()->RegisterMessageCallback(
      gcm_driver::kSetGcmInternalsRecording,
      base::BindRepeating(&GcmInternalsUIMessageHandler::SetRecording,
                          weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace

GCMInternalsUI::GCMInternalsUI(web::WebUIIOS* web_ui)
    : web::WebUIIOSController(web_ui) {
  // Set up the chrome://gcm-internals source.
  web::WebUIIOSDataSource* html_source =
      web::WebUIIOSDataSource::Create(kChromeUIGCMInternalsHost);

  html_source->UseStringsJs();

  // Add required resources.
  html_source->AddResourcePath(gcm_driver::kGcmInternalsCSS,
                               IDR_GCM_DRIVER_GCM_INTERNALS_CSS);
  html_source->AddResourcePath(gcm_driver::kGcmInternalsJS,
                               IDR_GCM_DRIVER_GCM_INTERNALS_JS);
  html_source->SetDefaultResource(IDR_GCM_DRIVER_GCM_INTERNALS_HTML);

  web::WebUIIOSDataSource::Add(ios::ChromeBrowserState::FromWebUIIOS(web_ui),
                               html_source);

  web_ui->AddMessageHandler(std::make_unique<GcmInternalsUIMessageHandler>());
}

GCMInternalsUI::~GCMInternalsUI() {}
