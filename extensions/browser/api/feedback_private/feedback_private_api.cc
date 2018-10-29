// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/feedback_private/feedback_private_api.h"

#include <string>
#include <utility>
#include <vector>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/statistics_recorder.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/feedback/system_logs/system_logs_fetcher.h"
#include "components/feedback/tracing_manager.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/feedback_private/feedback_private_delegate.h"
#include "extensions/browser/api/feedback_private/feedback_service.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/api/feedback_private.h"
#include "extensions/common/constants.h"

#if defined(OS_CHROMEOS)
#include "extensions/browser/api/feedback_private/log_source_access_manager.h"
#endif  // defined(OS_CHROMEOS)

using extensions::api::feedback_private::SystemInformation;
using feedback::FeedbackData;

namespace extensions {

namespace feedback_private = api::feedback_private;

using feedback_private::FeedbackInfo;
using feedback_private::FeedbackFlow;
using feedback_private::LogSource;
using feedback_private::SystemInformation;

using SystemInformationList =
    std::vector<api::feedback_private::SystemInformation>;

static base::LazyInstance<BrowserContextKeyedAPIFactory<FeedbackPrivateAPI>>::
    DestructorAtExit g_factory = LAZY_INSTANCE_INITIALIZER;

namespace {

constexpr base::FilePath::CharType kBluetoothLogsFilePath[] =
    FILE_PATH_LITERAL("/var/log/bluetooth/log.bz2");

constexpr char kBluetoothLogsAttachmentName[] = "bluetooth_logs.bz2";

// Getting the filename of a blob prepends a "C:\fakepath" to the filename.
// This is undesirable, strip it if it exists.
std::string StripFakepath(const std::string& path) {
  constexpr char kFakePathStr[] = "C:\\fakepath\\";
  if (base::StartsWith(path, kFakePathStr,
                       base::CompareCase::INSENSITIVE_ASCII))
    return path.substr(arraysize(kFakePathStr) - 1);
  return path;
}

// Returns the type of the landing page which is shown to the user when the
// report is successfully sent.
feedback_private::LandingPageType GetLandingPageType(const std::string& email) {
#if defined(OS_CHROMEOS)
  const std::string board =
      base::ToLowerASCII(base::SysInfo::GetLsbReleaseBoard());
  if (board.find("eve") == std::string::npos)
    return feedback_private::LANDING_PAGE_TYPE_NORMAL;

  if (!base::EndsWith(email, "@google.com",
                      base::CompareCase::INSENSITIVE_ASCII)) {
    return feedback_private::LANDING_PAGE_TYPE_NORMAL;
  }

  return feedback_private::LANDING_PAGE_TYPE_TECHSTOP;
#else
  return feedback_private::LANDING_PAGE_TYPE_NORMAL;
#endif  // defined(OS_CHROMEOS)
}

}  // namespace

// static
BrowserContextKeyedAPIFactory<FeedbackPrivateAPI>*
FeedbackPrivateAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

FeedbackPrivateAPI::FeedbackPrivateAPI(content::BrowserContext* context)
    : browser_context_(context),
#if !defined(OS_CHROMEOS)
      service_(new FeedbackService(context)) {
#else
      service_(new FeedbackService(context)),
      log_source_access_manager_(new LogSourceAccessManager(context)){
#endif  // defined(OS_CHROMEOS)
}

FeedbackPrivateAPI::~FeedbackPrivateAPI() {}

FeedbackService* FeedbackPrivateAPI::GetService() const {
  return service_.get();
}

#if defined(OS_CHROMEOS)
LogSourceAccessManager* FeedbackPrivateAPI::GetLogSourceAccessManager() const {
  return log_source_access_manager_.get();
}
#endif

void FeedbackPrivateAPI::RequestFeedbackForFlow(
    const std::string& description_template,
    const std::string& description_placeholder_text,
    const std::string& category_tag,
    const std::string& extra_diagnostics,
    const GURL& page_url,
    api::feedback_private::FeedbackFlow flow) {
  if (browser_context_ && EventRouter::Get(browser_context_)) {
    FeedbackInfo info;
    info.description = description_template;
    info.description_placeholder =
        std::make_unique<std::string>(description_placeholder_text);
    info.category_tag = std::make_unique<std::string>(category_tag);
    info.page_url = std::make_unique<std::string>(page_url.spec());
    info.system_information = std::make_unique<SystemInformationList>();

    // Any extra diagnostics information should be added to the sys info.
    if (!extra_diagnostics.empty()) {
      SystemInformation extra_info;
      extra_info.key = "EXTRA_DIAGNOSTICS";
      extra_info.value = extra_diagnostics;
      info.system_information->emplace_back(std::move(extra_info));
    }

    // The manager is only available if tracing is enabled.
    if (TracingManager* manager = TracingManager::Get()) {
      info.trace_id = std::make_unique<int>(manager->RequestTrace());
    }
    info.flow = flow;
#if defined(OS_MACOSX)
    const bool use_system_window_frame = true;
#else
    const bool use_system_window_frame = false;
#endif
    info.use_system_window_frame =
        std::make_unique<bool>(use_system_window_frame);

    std::unique_ptr<base::ListValue> args =
        feedback_private::OnFeedbackRequested::Create(info);

    auto event = std::make_unique<Event>(
        events::FEEDBACK_PRIVATE_ON_FEEDBACK_REQUESTED,
        feedback_private::OnFeedbackRequested::kEventName, std::move(args),
        browser_context_);

    // TODO(weidongg/754329): Using DispatchEventWithLazyListener() is a
    // temporary fix to the bug. Investigate a better solution that applies to
    // all scenarios.
    EventRouter::Get(browser_context_)
        ->DispatchEventWithLazyListener(extension_misc::kFeedbackExtensionId,
                                        std::move(event));
  }
}

// static
base::Closure* FeedbackPrivateGetStringsFunction::test_callback_ = NULL;

ExtensionFunction::ResponseAction FeedbackPrivateGetStringsFunction::Run() {
  auto params = feedback_private::GetStrings::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params.get());

  FeedbackPrivateDelegate* feedback_private_delegate =
      ExtensionsAPIClient::Get()->GetFeedbackPrivateDelegate();
  DCHECK(feedback_private_delegate);
  std::unique_ptr<base::DictionaryValue> dict =
      feedback_private_delegate->GetStrings(
          browser_context(),
          params->flow == FeedbackFlow::FEEDBACK_FLOW_SADTABCRASH);

  if (test_callback_ && !test_callback_->is_null())
    test_callback_->Run();

  return RespondNow(OneArgument(std::move(dict)));
}

ExtensionFunction::ResponseAction FeedbackPrivateGetUserEmailFunction::Run() {
  FeedbackPrivateDelegate* feedback_private_delegate =
      ExtensionsAPIClient::Get()->GetFeedbackPrivateDelegate();
  return RespondNow(OneArgument(std::make_unique<base::Value>(
      feedback_private_delegate->GetSignedInUserEmail(browser_context()))));
}

ExtensionFunction::ResponseAction
FeedbackPrivateGetSystemInformationFunction::Run() {
  // Self-deleting object.
  system_logs::SystemLogsFetcher* fetcher =
      ExtensionsAPIClient::Get()
          ->GetFeedbackPrivateDelegate()
          ->CreateSystemLogsFetcher(browser_context());
  fetcher->Fetch(base::Bind(
      &FeedbackPrivateGetSystemInformationFunction::OnCompleted, this));

  return RespondLater();
}

void FeedbackPrivateGetSystemInformationFunction::OnCompleted(
    std::unique_ptr<system_logs::SystemLogsResponse> sys_info) {
  SystemInformationList sys_info_list;
  if (sys_info) {
    sys_info_list.reserve(sys_info->size());
    for (auto& itr : *sys_info) {
      SystemInformation sys_info_entry;
      sys_info_entry.key = std::move(itr.first);
      sys_info_entry.value = std::move(itr.second);
      sys_info_list.emplace_back(std::move(sys_info_entry));
    }
  }

  Respond(ArgumentList(
      feedback_private::GetSystemInformation::Results::Create(sys_info_list)));
}

ExtensionFunction::ResponseAction FeedbackPrivateReadLogSourceFunction::Run() {
#if defined(OS_CHROMEOS)
  using Params = feedback_private::ReadLogSource::Params;
  std::unique_ptr<Params> api_params = Params::Create(*args_);

  LogSourceAccessManager* log_source_manager =
      FeedbackPrivateAPI::GetFactoryInstance()
          ->Get(browser_context())
          ->GetLogSourceAccessManager();

  if (!log_source_manager->FetchFromSource(
          api_params->params, extension_id(),
          base::Bind(&FeedbackPrivateReadLogSourceFunction::OnCompleted,
                     this))) {
    return RespondNow(Error(base::StringPrintf(
        "Unable to initiate fetch from log source %s.",
        feedback_private::ToString(api_params->params.source))));
  }

  return RespondLater();
#else
  NOTREACHED() << "API function is not supported on this platform.";
  return RespondNow(Error("API function is not supported on this platform."));
#endif  // defined(OS_CHROMEOS)
}

#if defined(OS_CHROMEOS)
void FeedbackPrivateReadLogSourceFunction::OnCompleted(
    std::unique_ptr<feedback_private::ReadLogSourceResult> result) {
  Respond(
      ArgumentList(feedback_private::ReadLogSource::Results::Create(*result)));
}
#endif  // defined(OS_CHROMEOS)

ExtensionFunction::ResponseAction FeedbackPrivateSendFeedbackFunction::Run() {
  std::unique_ptr<feedback_private::SendFeedback::Params> params(
      feedback_private::SendFeedback::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  const FeedbackInfo& feedback_info = params->feedback;

  // Populate feedback data.
  FeedbackPrivateDelegate* delegate =
      ExtensionsAPIClient::Get()->GetFeedbackPrivateDelegate();
  scoped_refptr<FeedbackData> feedback_data =
      base::MakeRefCounted<FeedbackData>(
          delegate->GetFeedbackUploaderForContext(browser_context()));
  feedback_data->set_context(browser_context());
  feedback_data->set_description(feedback_info.description);

  if (feedback_info.product_id)
    feedback_data->set_product_id(*feedback_info.product_id);
  if (feedback_info.category_tag)
    feedback_data->set_category_tag(*feedback_info.category_tag);
  if (feedback_info.page_url)
    feedback_data->set_page_url(*feedback_info.page_url);
  if (feedback_info.email)
    feedback_data->set_user_email(*feedback_info.email);
  if (feedback_info.trace_id)
    feedback_data->set_trace_id(*feedback_info.trace_id);

  if (feedback_info.attached_file_blob_uuid &&
      !feedback_info.attached_file_blob_uuid->empty()) {
    feedback_data->set_attached_filename(
        StripFakepath((*feedback_info.attached_file).name));
    feedback_data->set_attached_file_uuid(
        *feedback_info.attached_file_blob_uuid);
  }

  if (feedback_info.screenshot_blob_uuid &&
      !feedback_info.screenshot_blob_uuid->empty()) {
    feedback_data->set_screenshot_uuid(*feedback_info.screenshot_blob_uuid);
  }

  auto sys_logs = std::make_unique<FeedbackData::SystemLogsMap>();
  const SystemInformationList* sys_info =
      feedback_info.system_information.get();
  if (sys_info) {
    for (const SystemInformation& info : *sys_info)
      sys_logs->emplace(info.key, info.value);
  }

#if defined(OS_CHROMEOS)
  delegate->FetchAndMergeIwlwifiDumpLogsIfPresent(
      std::move(sys_logs), browser_context(),
      base::Bind(&FeedbackPrivateSendFeedbackFunction::OnAllLogsFetched, this,
                 feedback_data, feedback_info.send_histograms,
                 feedback_info.send_bluetooth_logs &&
                     *feedback_info.send_bluetooth_logs));
#else
  OnAllLogsFetched(feedback_data, feedback_info.send_histograms,
                   false /* send_bluetooth_logs */, std::move(sys_logs));
#endif  // defined(OS_CHROMEOS)

  return RespondLater();
}

void FeedbackPrivateSendFeedbackFunction::OnAllLogsFetched(
    scoped_refptr<FeedbackData> feedback_data,
    bool send_histograms,
    bool send_bluetooth_logs,
    std::unique_ptr<system_logs::SystemLogsResponse> sys_logs) {
  feedback_data->SetAndCompressSystemInfo(std::move(sys_logs));

  FeedbackService* service = FeedbackPrivateAPI::GetFactoryInstance()
                                 ->Get(browser_context())
                                 ->GetService();
  DCHECK(service);

  if (send_histograms) {
    auto histograms = std::make_unique<std::string>();
    *histograms =
        base::StatisticsRecorder::ToJSON(base::JSON_VERBOSITY_LEVEL_FULL);
    if (!histograms->empty())
      feedback_data->SetAndCompressHistograms(std::move(histograms));
  }

  if (send_bluetooth_logs) {
    std::unique_ptr<std::string> bluetooth_logs =
        std::make_unique<std::string>();
    if (base::ReadFileToString(base::FilePath(kBluetoothLogsFilePath),
                               bluetooth_logs.get())) {
      feedback_data->AddFile(kBluetoothLogsAttachmentName,
                             std::move(bluetooth_logs));
    }
  }

  service->SendFeedback(
      feedback_data,
      base::Bind(&FeedbackPrivateSendFeedbackFunction::OnCompleted, this,
                 GetLandingPageType(feedback_data->user_email())));
}

void FeedbackPrivateSendFeedbackFunction::OnCompleted(
    api::feedback_private::LandingPageType type,
    bool success) {
  Respond(TwoArguments(
      std::make_unique<base::Value>(feedback_private::ToString(
          success ? feedback_private::STATUS_SUCCESS
                  : feedback_private::STATUS_DELAYED)),
      std::make_unique<base::Value>(feedback_private::ToString(type))));
  if (!success) {
    ExtensionsAPIClient::Get()
        ->GetFeedbackPrivateDelegate()
        ->NotifyFeedbackDelayed();
  }
}

ExtensionFunction::ResponseAction
FeedbackPrivateLogSrtPromptResultFunction::Run() {
  std::unique_ptr<feedback_private::LogSrtPromptResult::Params> params(
      feedback_private::LogSrtPromptResult::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  const feedback_private::SrtPromptResult result = params->result;

  switch (result) {
    case feedback_private::SRT_PROMPT_RESULT_ACCEPTED:
      base::RecordAction(base::UserMetricsAction("Feedback.SrtPromptAccepted"));
      break;
    case feedback_private::SRT_PROMPT_RESULT_DECLINED:
      base::RecordAction(base::UserMetricsAction("Feedback.SrtPromptDeclined"));
      break;
    case feedback_private::SRT_PROMPT_RESULT_CLOSED:
      base::RecordAction(base::UserMetricsAction("Feedback.SrtPromptClosed"));
      break;
    default:
      return RespondNow(Error("Invalid arugment."));
  }
  return RespondNow(NoArguments());
}

}  // namespace extensions
