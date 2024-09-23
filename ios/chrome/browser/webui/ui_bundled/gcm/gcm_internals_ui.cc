// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/webui/ui_bundled/gcm/gcm_internals_ui.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/gcm_driver/gcm_client.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/gcm_internals_constants.h"
#include "components/gcm_driver/gcm_internals_helper.h"
#include "components/gcm_driver/gcm_profile_service.h"
#include "components/grit/dev_ui_components_resources.h"
#include "ios/chrome/browser/gcm/model/ios_chrome_gcm_profile_service_factory.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#include "ios/web/public/webui/web_ui_ios.h"
#include "ios/web/public/webui/web_ui_ios_data_source.h"
#include "ios/web/public/webui/web_ui_ios_message_handler.h"

namespace {

// Class acting as a controller of the chrome://gcm-internals WebUI.
class GcmInternalsUIMessageHandler : public web::WebUIIOSMessageHandler {
 public:
  GcmInternalsUIMessageHandler();

  GcmInternalsUIMessageHandler(const GcmInternalsUIMessageHandler&) = delete;
  GcmInternalsUIMessageHandler& operator=(const GcmInternalsUIMessageHandler&) =
      delete;

  ~GcmInternalsUIMessageHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

 private:
  // Return all of the GCM related infos to the gcm-internals page by calling
  // Javascript callback function `gcm-internals.returnInfo()`.
  void ReturnResults(PrefService* prefs,
                     gcm::GCMProfileService* profile_service,
                     const gcm::GCMClient::GCMStatistics* stats) const;

  // Request all of the GCM related infos through gcm profile service.
  void RequestAllInfo(const base::Value::List& args);

  // Enables/disables GCM activity recording through gcm profile service.
  void SetRecording(const base::Value::List& args);

  // Callback function of the request for all gcm related infos.
  void RequestGCMStatisticsFinished(
      const gcm::GCMClient::GCMStatistics& args) const;

  // Factory for creating references in callbacks.
  base::WeakPtrFactory<GcmInternalsUIMessageHandler> weak_ptr_factory_;
};

GcmInternalsUIMessageHandler::GcmInternalsUIMessageHandler()
    : weak_ptr_factory_(this) {}

GcmInternalsUIMessageHandler::~GcmInternalsUIMessageHandler() {}

void GcmInternalsUIMessageHandler::ReturnResults(
    PrefService* prefs,
    gcm::GCMProfileService* profile_service,
    const gcm::GCMClient::GCMStatistics* stats) const {
  base::Value::Dict results =
      gcm_driver::SetGCMInternalsInfo(stats, profile_service, prefs);

  base::Value event_name(gcm_driver::kSetGcmInternalsInfo);
  base::ValueView args[] = {event_name, results};
  web_ui()->CallJavascriptFunction("cr.webUIListenerCallback", args);
}

void GcmInternalsUIMessageHandler::RequestAllInfo(
    const base::Value::List& args) {
  if (args.size() != 1 || !args[0].is_bool()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  bool clear_logs = args[0].GetBool();

  gcm::GCMDriver::ClearActivityLogs clear_activity_logs =
      clear_logs ? gcm::GCMDriver::CLEAR_LOGS : gcm::GCMDriver::KEEP_LOGS;

  ProfileIOS* profile = ProfileIOS::FromWebUIIOS(web_ui());
  gcm::GCMProfileService* profile_service =
      IOSChromeGCMProfileServiceFactory::GetForProfile(profile);

  if (!profile_service || !profile_service->driver()) {
    ReturnResults(profile->GetPrefs(), nullptr, nullptr);
  } else {
    profile_service->driver()->GetGCMStatistics(
        base::BindOnce(
            &GcmInternalsUIMessageHandler::RequestGCMStatisticsFinished,
            weak_ptr_factory_.GetWeakPtr()),
        clear_activity_logs);
  }
}

void GcmInternalsUIMessageHandler::SetRecording(const base::Value::List& args) {
  if (args.size() != 1 || !args[0].is_bool()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  bool recording = args[0].GetBool();

  ProfileIOS* profile = ProfileIOS::FromWebUIIOS(web_ui());
  gcm::GCMProfileService* profile_service =
      IOSChromeGCMProfileServiceFactory::GetForProfile(profile);

  if (!profile_service) {
    ReturnResults(profile->GetPrefs(), nullptr, nullptr);
    return;
  }
  // Get fresh stats after changing recording setting.
  profile_service->driver()->SetGCMRecording(
      base::BindRepeating(
          &GcmInternalsUIMessageHandler::RequestGCMStatisticsFinished,
          weak_ptr_factory_.GetWeakPtr()),
      recording);
}

void GcmInternalsUIMessageHandler::RequestGCMStatisticsFinished(
    const gcm::GCMClient::GCMStatistics& stats) const {
  ProfileIOS* profile = ProfileIOS::FromWebUIIOS(web_ui());
  DCHECK(profile);
  gcm::GCMProfileService* profile_service =
      IOSChromeGCMProfileServiceFactory::GetForProfile(profile);

  DCHECK(profile_service);
  ReturnResults(profile->GetPrefs(), profile_service, &stats);
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

GCMInternalsUI::GCMInternalsUI(web::WebUIIOS* web_ui, const std::string& host)
    : web::WebUIIOSController(web_ui, host) {
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

  web::WebUIIOSDataSource::Add(ProfileIOS::FromWebUIIOS(web_ui), html_source);

  web_ui->AddMessageHandler(std::make_unique<GcmInternalsUIMessageHandler>());
}

GCMInternalsUI::~GCMInternalsUI() {}
