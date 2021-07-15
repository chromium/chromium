// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/feedback_private/feedback_service.h"

#include <memory>
#include <string>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/statistics_recorder.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "components/feedback/feedback_data.h"
#include "components/feedback/feedback_report.h"
#include "components/feedback/system_logs/system_logs_fetcher.h"
#include "components/feedback/system_logs/system_logs_source.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/feedback_private/feedback_private_delegate.h"
#include "extensions/browser/blob_reader.h"
#include "net/base/network_change_notifier.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "chromeos/services/assistant/public/cpp/assistant_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace extensions {

using system_logs::SysLogsFetcherCallback;
using system_logs::SystemLogsFetcher;
using system_logs::SystemLogsResponse;

namespace {

constexpr base::FilePath::CharType kBluetoothLogsFilePath[] =
    FILE_PATH_LITERAL("/var/log/bluetooth/log.bz2");
constexpr base::FilePath::CharType kBluetoothLogsFilePathOld[] =
    FILE_PATH_LITERAL("/var/log/bluetooth/log.bz2.old");
constexpr base::FilePath::CharType kBluetoothQualityReportFilePath[] =
    FILE_PATH_LITERAL("/var/log/bluetooth/bluetooth_quality_report");

constexpr char kBluetoothLogsAttachmentName[] = "bluetooth_logs.bz2";
constexpr char kBluetoothLogsAttachmentNameOld[] = "bluetooth_logs.old.bz2";
constexpr char kBluetoothQualityReportAttachmentName[] =
    "bluetooth_quality_report";

#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char kLacrosHistogramsFilename[] = "lacros_histograms.zip";
#endif

}  // namespace

FeedbackService::FeedbackService(content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

FeedbackService::~FeedbackService() = default;

void FeedbackService::SendFeedback(
    const FeedbackParams& params,
    scoped_refptr<feedback::FeedbackData> feedback_data,
    SendFeedbackCallback callback) {
  auto* browser_client = ExtensionsBrowserClient::Get();
  feedback_data->set_locale(browser_client->GetApplicationLocale());
  feedback_data->set_user_agent(browser_client->GetUserAgent());

  FetchAttachedFileAndScreenshot(
      feedback_data,
      base::BindOnce(&FeedbackService::OnAttachedFileAndScreenshotFetched, this,
                     params, feedback_data, std::move(callback)));
}

void FeedbackService::FetchAttachedFileAndScreenshot(
    scoped_refptr<feedback::FeedbackData> feedback_data,
    base::OnceClosure callback) {
  const bool must_attach_file = !feedback_data->attached_file_uuid().empty();
  const bool must_attach_screenshot = !feedback_data->screenshot_uuid().empty();
  auto barrier_closure = base::BarrierClosure(
      (must_attach_file ? 1 : 0) + (must_attach_screenshot ? 1 : 0),
      std::move(callback));

  if (must_attach_file) {
    auto populate_attached_file = base::BindOnce(
        [](scoped_refptr<feedback::FeedbackData> feedback_data,
           std::unique_ptr<std::string> data, int64_t length) {
          feedback_data->set_attached_file_uuid(std::string());
          if (data)
            feedback_data->AttachAndCompressFileData(std::move(*data));
        },
        feedback_data);

    BlobReader::Read(browser_context_, feedback_data->attached_file_uuid(),
                     std::move(populate_attached_file).Then(barrier_closure));
  }

  if (must_attach_screenshot) {
    auto populate_screenshot = base::BindOnce(
        [](scoped_refptr<feedback::FeedbackData> feedback_data,
           std::unique_ptr<std::string> data, int64_t length) {
          feedback_data->set_screenshot_uuid(std::string());
          if (data)
            feedback_data->set_image(std::move(*data));
        },
        feedback_data);
    BlobReader::Read(browser_context_, feedback_data->screenshot_uuid(),
                     std::move(populate_screenshot).Then(barrier_closure));
  }
}

void FeedbackService::OnAttachedFileAndScreenshotFetched(
    const FeedbackParams& params,
    scoped_refptr<feedback::FeedbackData> feedback_data,
    SendFeedbackCallback callback) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  FetchExtraLogs(feedback_data,
                 base::BindOnce(&FeedbackService::OnExtraLogsFetched, this,
                                params, std::move(callback)));
#else
  OnAllLogsFetched(params, feedback_data, std::move(callback));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void FeedbackService::FetchExtraLogs(
    scoped_refptr<feedback::FeedbackData> feedback_data,
    FetchExtraLogsCallback callback) {
  FeedbackPrivateDelegate* feedback_private_delegate =
      ExtensionsAPIClient::Get()->GetFeedbackPrivateDelegate();
  feedback_private_delegate->FetchExtraLogs(feedback_data, std::move(callback));
}

void FeedbackService::OnExtraLogsFetched(
    const FeedbackParams& params,
    SendFeedbackCallback callback,
    scoped_refptr<feedback::FeedbackData> feedback_data) {
  FetchLacrosHistograms(
      base::BindOnce(&FeedbackService::OnLacrosHistogramsFetched, this, params,
                     feedback_data, std::move(callback)));
}

void FeedbackService::FetchLacrosHistograms(GetHistogramsCallback callback) {
  FeedbackPrivateDelegate* feedback_private_delegate =
      ExtensionsAPIClient::Get()->GetFeedbackPrivateDelegate();
  feedback_private_delegate->GetLacrosHistograms(std::move(callback));
}

void FeedbackService::OnLacrosHistogramsFetched(
    const FeedbackParams& params,
    scoped_refptr<feedback::FeedbackData> feedback_data,
    SendFeedbackCallback callback,
    const std::string& compressed_histograms) {
  if (!compressed_histograms.empty()) {
    feedback_data->AddFile(kLacrosHistogramsFilename,
                           std::move(compressed_histograms));
  }
  OnAllLogsFetched(params, feedback_data, std::move(callback));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void FeedbackService::OnAllLogsFetched(
    const FeedbackParams& params,
    scoped_refptr<feedback::FeedbackData> feedback_data,
    SendFeedbackCallback callback) {
  if (!params.send_tab_titles) {
    feedback_data->RemoveLog(
        feedback::FeedbackReport::kMemUsageWithTabTitlesKey);
  }
  feedback_data->CompressSystemInfo();

  if (params.send_histograms) {
    std::string histograms =
        base::StatisticsRecorder::ToJSON(base::JSON_VERBOSITY_LEVEL_FULL);
    feedback_data->SetAndCompressHistograms(std::move(histograms));
  }

  if (params.send_bluetooth_logs) {
    std::string bluetooth_logs;
    if (base::ReadFileToString(base::FilePath(kBluetoothLogsFilePath),
                               &bluetooth_logs)) {
      feedback_data->AddFile(kBluetoothLogsAttachmentName,
                             std::move(bluetooth_logs));
    }
    if (base::ReadFileToString(base::FilePath(kBluetoothLogsFilePathOld),
                               &bluetooth_logs)) {
      feedback_data->AddFile(kBluetoothLogsAttachmentNameOld,
                             std::move(bluetooth_logs));
    }
    if (base::ReadFileToString(base::FilePath(kBluetoothQualityReportFilePath),
                               &bluetooth_logs)) {
      feedback_data->AddFile(kBluetoothQualityReportAttachmentName,
                             std::move(bluetooth_logs));
    }
  }

  DCHECK(feedback_data->attached_file_uuid().empty());
  DCHECK(feedback_data->screenshot_uuid().empty());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Send feedback to Assistant server if triggered from Google Assistant.
  if (feedback_data->from_assistant()) {
    ash::AssistantController::Get()->SendAssistantFeedback(
        feedback_data->assistant_debug_info_allowed(),
        feedback_data->description(), feedback_data->image());
  }
#endif

  // Signal the feedback object that the data from the feedback page has been
  // filled - the object will manage sending of the actual report.
  feedback_data->OnFeedbackPageDataComplete();

  // True means report will be sent shortly.
  // False means report will be sent once the device is online.
  const bool status = !net::NetworkChangeNotifier::IsOffline();
  std::move(callback).Run(status);
}

}  // namespace extensions
