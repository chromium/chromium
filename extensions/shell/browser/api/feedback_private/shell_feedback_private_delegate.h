// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_API_FEEDBACK_PRIVATE_SHELL_FEEDBACK_PRIVATE_DELEGATE_H_
#define EXTENSIONS_SHELL_BROWSER_API_FEEDBACK_PRIVATE_SHELL_FEEDBACK_PRIVATE_DELEGATE_H_

#include "components/feedback/feedback_data.h"
#include "extensions/browser/api/feedback_private/feedback_private_delegate.h"

#include "base/macros.h"

namespace extensions {

class ShellFeedbackPrivateDelegate : public FeedbackPrivateDelegate {
 public:
  ShellFeedbackPrivateDelegate();
  ~ShellFeedbackPrivateDelegate() override;

  // FeedbackPrivateDelegate:
  std::unique_ptr<base::DictionaryValue> GetStrings(
      content::BrowserContext* browser_context,
      bool from_crash) const override;
  system_logs::SystemLogsFetcher* CreateSystemLogsFetcher(
      content::BrowserContext* context) const override;
#if defined(OS_CHROMEOS)
  std::unique_ptr<system_logs::SystemLogsSource> CreateSingleLogSource(
      api::feedback_private::LogSource source_type) const override;
  void FetchExtraLogs(scoped_refptr<feedback::FeedbackData> feedback_data,
                      FetchExtraLogsCallback callback) const override;
  void UnloadFeedbackExtension(content::BrowserContext* context) const override;
  api::feedback_private::LandingPageType GetLandingPageType(
      const feedback::FeedbackData& feedback_data) const override;
#endif  // defined(OS_CHROMEOS)
  std::string GetSignedInUserEmail(
      content::BrowserContext* context) const override;
  void NotifyFeedbackDelayed() const override;
  feedback::FeedbackUploader* GetFeedbackUploaderForContext(
      content::BrowserContext* context) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellFeedbackPrivateDelegate);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_API_FEEDBACK_PRIVATE_SHELL_FEEDBACK_PRIVATE_DELEGATE_H_
