// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/feedback_private/feedback_service.h"

#include <memory>
#include <string>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/blob_reader.h"
#include "extensions/browser/extensions_browser_client.h"
#include "net/base/network_change_notifier.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "chromeos/services/assistant/public/cpp/assistant_service.h"
#include "extensions/browser/api/feedback_private/log_source_access_manager.h"
#include "mojo/public/cpp/bindings/remote.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace extensions {

FeedbackService::FeedbackService(content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

FeedbackService::~FeedbackService() = default;

void FeedbackService::SendFeedback(
    scoped_refptr<feedback::FeedbackData> feedback_data,
    SendFeedbackCallback callback) {
  auto* browser_client = ExtensionsBrowserClient::Get();
  feedback_data->set_locale(browser_client->GetApplicationLocale());
  feedback_data->set_user_agent(browser_client->GetUserAgent());

  // CompleteSendFeedback must be called once the attached file and screenshot
  // have been read, if applicable. The barrier closure will call this when its
  // count of remaining tasks has been reduced to zero (immediately, if none are
  // there in the first place).
  const bool must_attach_file = !feedback_data->attached_file_uuid().empty();
  const bool must_attach_screenshot = !feedback_data->screenshot_uuid().empty();
  auto barrier_closure = base::BarrierClosure(
      (must_attach_file ? 1 : 0) + (must_attach_screenshot ? 1 : 0),
      base::BindOnce(&FeedbackService::CompleteSendFeedback, AsWeakPtr(),
                     feedback_data, std::move(callback)));

  if (must_attach_file) {
    auto populate_attached_file = base::BindOnce(
        [](scoped_refptr<feedback::FeedbackData> feedback_data,
           std::unique_ptr<std::string> data, int64_t /* total_blob_length */) {
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
           std::unique_ptr<std::string> data, int64_t /* total_blob_length */) {
          feedback_data->set_screenshot_uuid(std::string());
          if (data)
            feedback_data->set_image(std::move(*data));
        },
        feedback_data);
    BlobReader::Read(browser_context_, feedback_data->screenshot_uuid(),
                     std::move(populate_screenshot).Then(barrier_closure));
  }
}

void FeedbackService::CompleteSendFeedback(
    scoped_refptr<feedback::FeedbackData> feedback_data,
    SendFeedbackCallback callback) {
  // A particular data collection is considered completed if,
  // a.) The blob URL is invalid - this will either happen because we never had
  //     a URL and never needed to read this data, or that the data read failed
  //     and we set it to invalid in the data read callback.
  // b.) The associated data object exists, meaning that the data has been read
  //     and the read callback has updated the associated data on the feedback
  //     object.
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

  // Sending the feedback will be delayed if the user is offline.
  const bool result = !net::NetworkChangeNotifier::IsOffline();

  // TODO(rkc): Change this once we have FeedbackData/Util refactored to
  // report the status of the report being sent.
  std::move(callback).Run(result);
}

}  // namespace extensions
