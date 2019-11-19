// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/api/feedback_private/shell_feedback_private_delegate.h"

#include <string>

#include "base/logging.h"
#include "base/values.h"
#include "components/feedback/feedback_uploader.h"
#include "components/feedback/feedback_uploader_factory.h"
#include "components/feedback/system_logs/system_logs_fetcher.h"
#include "components/feedback/system_logs/system_logs_source.h"
#include "content/public/browser/browser_context.h"
#include "extensions/shell/browser/system_logs/shell_system_logs_fetcher.h"

namespace extensions {

ShellFeedbackPrivateDelegate::ShellFeedbackPrivateDelegate() = default;
ShellFeedbackPrivateDelegate::~ShellFeedbackPrivateDelegate() = default;

std::unique_ptr<base::DictionaryValue> ShellFeedbackPrivateDelegate::GetStrings(
    content::BrowserContext* browser_context,
    bool from_crash) const {
  NOTIMPLEMENTED();
  return nullptr;
}

system_logs::SystemLogsFetcher*
ShellFeedbackPrivateDelegate::CreateSystemLogsFetcher(
    content::BrowserContext* context) const {
  return system_logs::BuildShellSystemLogsFetcher(context);
}

#if defined(OS_CHROMEOS)
std::unique_ptr<system_logs::SystemLogsSource>
ShellFeedbackPrivateDelegate::CreateSingleLogSource(
    api::feedback_private::LogSource source_type) const {
  NOTIMPLEMENTED();
  return nullptr;
}

void ShellFeedbackPrivateDelegate::FetchExtraLogs(
    scoped_refptr<feedback::FeedbackData> feedback_data,
    FetchExtraLogsCallback callback) const {
  NOTIMPLEMENTED();
  std::move(callback).Run(feedback_data);
}

void ShellFeedbackPrivateDelegate::UnloadFeedbackExtension(
    content::BrowserContext* context) const {
  NOTIMPLEMENTED();
}

api::feedback_private::LandingPageType
ShellFeedbackPrivateDelegate::GetLandingPageType(
    const feedback::FeedbackData& feedback_data) const {
  return api::feedback_private::LANDING_PAGE_TYPE_NOLANDINGPAGE;
}
#endif

std::string ShellFeedbackPrivateDelegate::GetSignedInUserEmail(
    content::BrowserContext* context) const {
  return std::string();
}

void ShellFeedbackPrivateDelegate::NotifyFeedbackDelayed() const {}

feedback::FeedbackUploader*
ShellFeedbackPrivateDelegate::GetFeedbackUploaderForContext(
    content::BrowserContext* context) const {
  return feedback::FeedbackUploaderFactory::GetForBrowserContext(context);
}

}  // namespace extensions
