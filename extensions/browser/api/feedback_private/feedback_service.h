// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_FEEDBACK_SERVICE_H_
#define EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_FEEDBACK_SERVICE_H_

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "components/feedback/system_logs/system_logs_fetcher.h"
#include "extensions/browser/api/feedback_private/feedback_private_delegate.h"

namespace feedback {
class FeedbackData;
}  // namespace feedback

namespace extensions {

// The FeedbackParams holds parameters specific to a feedback report.
struct FeedbackParams {
  // The user has a @google.com email or not.
  bool is_internal_email = false;
  // If true, include the browser tab titles in the feedback.
  bool send_tab_titles = false;
  // If true, include histograms in the feedback.
  bool send_histograms = false;
  // If true, include bluetooth logs in the feedback.
  bool send_bluetooth_logs = false;
};

// Callback invoked when the feedback report is ready to be sent.
// True will be passed to indicate that it is being successfully sent now,
// and false to indicate that it will be delayed due to being offline.
using SendFeedbackCallback = base::OnceCallback<void(bool)>;

// The feedback service provides the ability to gather the various pieces of
// data needed to send a feedback report and then send the report once all
// the pieces are available.
class FeedbackService : public base::RefCountedThreadSafe<FeedbackService> {
 public:
  explicit FeedbackService(content::BrowserContext* browser_context);
  FeedbackService(const FeedbackService&) = delete;
  FeedbackService& operator=(const FeedbackService&) = delete;

  virtual void SendFeedback(const FeedbackParams& params,
                            scoped_refptr<feedback::FeedbackData> feedback_data,
                            SendFeedbackCallback callback);

 protected:
  virtual ~FeedbackService();

 private:
  friend class base::RefCountedThreadSafe<FeedbackService>;

  void FetchAttachedFileAndScreenshot(
      scoped_refptr<feedback::FeedbackData> feedback_data,
      base::OnceClosure callback);
  void OnAttachedFileAndScreenshotFetched(
      const FeedbackParams& params,
      scoped_refptr<feedback::FeedbackData> feedback_data,
      SendFeedbackCallback callback);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Gets logs that aren't covered by FetchSystemInformation, but should be
  // included in the feedback report. These currently consist of the Intel Wi-Fi
  // debug logs (if they exist).
  // Modifies |feedback_data| and passes it on to |callback|.
  void FetchExtraLogs(scoped_refptr<feedback::FeedbackData> feedback_data,
                      FetchExtraLogsCallback callback);
  void OnExtraLogsFetched(const FeedbackParams& params,
                          SendFeedbackCallback callback,
                          scoped_refptr<feedback::FeedbackData> feedback_data);

  using GetHistogramsCallback = base::OnceCallback<void(const std::string&)>;
  // Gets Lacros histograms in zip compressed format which will be attached
  // as a file in unified feedback report.
  void FetchLacrosHistograms(GetHistogramsCallback callback);
  void OnLacrosHistogramsFetched(
      const FeedbackParams& params,
      scoped_refptr<feedback::FeedbackData> feedback_data,
      SendFeedbackCallback callback,
      const std::string& compressed_histograms);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  void OnAllLogsFetched(const FeedbackParams& params,
                        scoped_refptr<feedback::FeedbackData> feedback_data,
                        SendFeedbackCallback callback);

  content::BrowserContext* browser_context_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_FEEDBACK_SERVICE_H_
