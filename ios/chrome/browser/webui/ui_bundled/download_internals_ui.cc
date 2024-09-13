// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/webui/ui_bundled/download_internals_ui.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/uuid.h"
#include "components/download/public/background_service/background_download_service.h"
#include "components/download/public/background_service/download_params.h"
#include "components/download/public/background_service/logger.h"
#include "components/grit/download_internals_resources.h"
#include "components/grit/download_internals_resources_map.h"
#include "ios/chrome/browser/download/model/background_service/background_download_service_factory.h"
#include "ios/chrome/browser/shared/model/profile/profile_ios.h"
#include "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#include "ios/web/public/webui/web_ui_ios.h"
#include "ios/web/public/webui/web_ui_ios_data_source.h"
#include "ios/web/public/webui/web_ui_ios_message_handler.h"

// DownloadInternalsUIMessageHandler.

// Glue code between web UI frontend and background download service native
// code.
class DownloadInternalsUIMessageHandler : public web::WebUIIOSMessageHandler,
                                          public download::Logger::Observer {
 public:
  DownloadInternalsUIMessageHandler() = default;

  DownloadInternalsUIMessageHandler(const DownloadInternalsUIMessageHandler&) =
      delete;
  void operator=(const DownloadInternalsUIMessageHandler&) = delete;
  ~DownloadInternalsUIMessageHandler() override {
    if (download_service_) {
      download_service_->GetLogger()->RemoveObserver(this);
    }
  }

 private:
  // WebUIIOSMessageHandler implementation.
  void RegisterMessages() override {
    web_ui()->RegisterMessageCallback(
        "getServiceStatus",
        base::BindRepeating(
            &DownloadInternalsUIMessageHandler::HandleGetServiceStatus,
            weak_ptr_factory_.GetWeakPtr()));
    web_ui()->RegisterMessageCallback(
        "getServiceDownloads",
        base::BindRepeating(
            &DownloadInternalsUIMessageHandler::HandleGetServiceDownloads,
            weak_ptr_factory_.GetWeakPtr()));
    web_ui()->RegisterMessageCallback(
        "startDownload",
        base::BindRepeating(
            &DownloadInternalsUIMessageHandler::HandleStartDownload,
            weak_ptr_factory_.GetWeakPtr()));

    ProfileIOS* profile = ProfileIOS::FromWebUIIOS(web_ui());
    download_service_ =
        BackgroundDownloadServiceFactory::GetForProfile(profile);

    // download_service_ will be null in incognito mode on iOS.
    if (download_service_)
      download_service_->GetLogger()->AddObserver(this);
  }

  // download::Logger::Observer implementation.
  void OnServiceStatusChanged(
      const base::Value::Dict& service_status) override {
    web_ui()->FireWebUIListener("service-status-changed", service_status);
  }

  void OnServiceDownloadsAvailable(
      const base::Value::List& service_downloads) override {
    web_ui()->FireWebUIListener("service-downloads-available",
                                service_downloads);
  }

  void OnServiceDownloadChanged(
      const base::Value::Dict& service_download) override {
    web_ui()->FireWebUIListener("service-download-changed", service_download);
  }

  void OnServiceDownloadFailed(
      const base::Value::Dict& service_download) override {
    web_ui()->FireWebUIListener("service-download-failed", service_download);
  }

  void OnServiceRequestMade(const base::Value::Dict& service_request) override {
    web_ui()->FireWebUIListener("service-request-made", service_request);
  }

  void HandleGetServiceStatus(const base::Value::List& args) {
    if (!download_service_)
      return;

    web_ui()->ResolveJavascriptCallback(
        args[0], download_service_->GetLogger()->GetServiceStatus());
  }

  void HandleGetServiceDownloads(const base::Value::List& args) {
    if (!download_service_)
      return;

    web_ui()->ResolveJavascriptCallback(
        args[0], download_service_->GetLogger()->GetServiceDownloads());
  }

  void HandleStartDownload(const base::Value::List& args) {
    if (!download_service_)
      return;

    CHECK_GT(args.size(), 1u) << "Missing argument download URL.";
    GURL url = GURL(args[1].GetString());
    if (!url.is_valid()) {
      LOG(WARNING) << "Can't parse download URL, try to enter a valid URL.";
      return;
    }

    download::DownloadParams params;
    params.guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
    params.client = download::DownloadClient::DEBUGGING;
    params.request_params.method = "GET";
    params.request_params.url = url;

    net::NetworkTrafficAnnotationTag traffic_annotation =
        net::DefineNetworkTrafficAnnotation("download_internals_webui_source",
                                            R"(
          semantics {
            sender: "Download Internals Page"
            description:
              "Starts a download with background download service in WebUI."
            trigger:
              "User clicks on the download button in "
              "chrome://download-internals."
            data: "None"
            destination: WEBSITE
          }
          policy {
            cookies_allowed: YES
            cookies_store: "user"
            setting: "This feature cannot be disabled by settings."
            policy_exception_justification: "Not implemented."
          })");

    params.traffic_annotation =
        net::MutableNetworkTrafficAnnotationTag(traffic_annotation);
    download_service_->StartDownload(std::move(params));
  }

  raw_ptr<download::BackgroundDownloadService> download_service_ = nullptr;

  base::WeakPtrFactory<DownloadInternalsUIMessageHandler> weak_ptr_factory_{
      this};
};

DownloadInternalsUI::DownloadInternalsUI(web::WebUIIOS* web_ui,
                                         const std::string& host)
    : web::WebUIIOSController(web_ui, host) {
  web_ui->AddMessageHandler(
      std::make_unique<DownloadInternalsUIMessageHandler>());

  web::WebUIIOSDataSource* html_source =
      web::WebUIIOSDataSource::Create(kChromeUIDownloadInternalsHost);
  html_source->UseStringsJs();
  html_source->AddResourcePaths(base::make_span(
      kDownloadInternalsResources, kDownloadInternalsResourcesSize));
  html_source->SetDefaultResource(
      IDR_DOWNLOAD_INTERNALS_DOWNLOAD_INTERNALS_HTML);
  web::WebUIIOSDataSource::Add(ProfileIOS::FromWebUIIOS(web_ui), html_source);
}

DownloadInternalsUI::~DownloadInternalsUI() = default;
