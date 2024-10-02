// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/api/feedback_private/shell_feedback_private_delegate.h"

#include <string>

#include "base/notreached.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "components/feedback/content/feedback_uploader_factory.h"
#include "components/feedback/feedback_uploader.h"
#include "components/feedback/system_logs/system_logs_fetcher.h"
#include "components/feedback/system_logs/system_logs_source.h"
#include "content/public/browser/browser_context.h"
#include "extensions/shell/browser/system_logs/shell_system_logs_fetcher.h"

namespace extensions {

ShellFeedbackPrivateDelegate::ShellFeedbackPrivateDelegate() = default;
ShellFeedbackPrivateDelegate::~ShellFeedbackPrivateDelegate() = default;

base::Value::Dict ShellFeedbackPrivateDelegate::GetStrings(
    content::BrowserContext* browser_context,
    bool from_crash) const {
  NOTIMPLEMENTED();
  return {};
}

void ShellFeedbackPrivateDelegate::FetchSystemInformation(
    content::BrowserContext* context,
    system_logs::SysLogsFetcherCallback callback) const {
  // self-deleting object
  auto* fetcher = system_logs::BuildShellSystemLogsFetcher(context);
  fetcher->Fetch(std::move(callback));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
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

api::feedback_private::LandingPageType
ShellFeedbackPrivateDelegate::GetLandingPageType(
    const feedback::FeedbackData& feedback_data) const {
  return api::feedback_private::LandingPageType::kNoLandingPage;
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

void ShellFeedbackPrivateDelegate::OpenFeedback(
    content::BrowserContext* context,
    api::feedback_private::FeedbackSource source) const {
  NOTIMPLEMENTED();
}

}  // namespace extensions
