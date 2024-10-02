// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_API_FEEDBACK_PRIVATE_SHELL_FEEDBACK_PRIVATE_DELEGATE_H_
#define EXTENSIONS_SHELL_BROWSER_API_FEEDBACK_PRIVATE_SHELL_FEEDBACK_PRIVATE_DELEGATE_H_

#include "components/feedback/feedback_data.h"
#include "extensions/browser/api/feedback_private/feedback_private_delegate.h"

#include "build/chromeos_buildflags.h"

namespace extensions {

class ShellFeedbackPrivateDelegate : public FeedbackPrivateDelegate {
 public:
  ShellFeedbackPrivateDelegate();

  ShellFeedbackPrivateDelegate(const ShellFeedbackPrivateDelegate&) = delete;
  ShellFeedbackPrivateDelegate& operator=(const ShellFeedbackPrivateDelegate&) =
      delete;

  ~ShellFeedbackPrivateDelegate() override;

  // FeedbackPrivateDelegate:
  base::Value::Dict GetStrings(content::BrowserContext* browser_context,
                               bool from_crash) const override;
  void FetchSystemInformation(
      content::BrowserContext* context,
      system_logs::SysLogsFetcherCallback callback) const override;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<system_logs::SystemLogsSource> CreateSingleLogSource(
      api::feedback_private::LogSource source_type) const override;
  void FetchExtraLogs(scoped_refptr<feedback::FeedbackData> feedback_data,
                      FetchExtraLogsCallback callback) const override;
  api::feedback_private::LandingPageType GetLandingPageType(
      const feedback::FeedbackData& feedback_data) const override;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  std::string GetSignedInUserEmail(
      content::BrowserContext* context) const override;
  void NotifyFeedbackDelayed() const override;
  feedback::FeedbackUploader* GetFeedbackUploaderForContext(
      content::BrowserContext* context) const override;
  void OpenFeedback(
      content::BrowserContext* context,
      api::feedback_private::FeedbackSource source) const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_API_FEEDBACK_PRIVATE_SHELL_FEEDBACK_PRIVATE_DELEGATE_H_
