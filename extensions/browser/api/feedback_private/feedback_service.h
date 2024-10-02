// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_FEEDBACK_SERVICE_H_
#define EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_FEEDBACK_SERVICE_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "components/feedback/system_logs/system_logs_fetcher.h"
#include "extensions/browser/api/feedback_private/feedback_private_delegate.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/components/dbus/debug_daemon/binary_log_files_reader.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace feedback {
class FeedbackData;
}  // namespace feedback

namespace extensions {

// The FeedbackParams holds parameters specific to a feedback report.
struct FeedbackParams {
  // The user has a @google.com email or not.
  bool is_internal_email = false;
  // Set this to true if system information should be loaded. If the data has
  // been pre-loaded into the feedback_data, this should be set to false.
  bool load_system_info = false;
  // If true, include the browser tab titles in the feedback.
  bool send_tab_titles = false;
  // If true, include histograms in the feedback.
  bool send_histograms = false;
  // If true, include bluetooth logs in the feedback.
  bool send_bluetooth_logs = false;
  // If true, include wifi debug logs in the feedback.
  bool send_wifi_debug_logs = false;
  // If true, include autofill metadata in the feedback.
  bool send_autofill_metadata = false;
  // The time when the feedback form submission was received by the backend.
  base::TimeTicks form_submit_time;
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
  FeedbackService(content::BrowserContext* browser_context,
                  FeedbackPrivateDelegate* delegate);
  FeedbackService(const FeedbackService&) = delete;
  FeedbackService& operator=(const FeedbackService&) = delete;

  virtual void RedactThenSendFeedback(
      const FeedbackParams& params,
      scoped_refptr<feedback::FeedbackData> feedback_data,
      SendFeedbackCallback callback);

  FeedbackPrivateDelegate* GetFeedbackPrivateDelegate() { return delegate_; }
#if BUILDFLAG(IS_CHROMEOS)
  void SetLogFilesRootPathForTesting(const base::FilePath& log_file_root);
#endif  // BUILDFLAG(IS_CHROMEOS)

 protected:
  virtual ~FeedbackService();

 private:
  friend class base::RefCountedThreadSafe<FeedbackService>;

  void SendFeedback(const FeedbackParams& params,
                    scoped_refptr<feedback::FeedbackData> feedback_data,
                    SendFeedbackCallback callback);

  void FetchAttachedFileAndScreenshot(
      scoped_refptr<feedback::FeedbackData> feedback_data,
      base::OnceClosure callback);
  void OnAttachedFileAndScreenshotFetched(
      const FeedbackParams& params,
      scoped_refptr<feedback::FeedbackData> feedback_data,
      SendFeedbackCallback callback);
  void FetchSystemInformation(
      const FeedbackParams& params,
      scoped_refptr<feedback::FeedbackData> feedback_data);
  void OnSystemInformationFetched(
      base::TimeTicks fetch_start_time,
      const FeedbackParams& params,
      scoped_refptr<feedback::FeedbackData> feedback_data,
      std::unique_ptr<system_logs::SystemLogsResponse> sys_info);
  void OnAllLogsFetched(const FeedbackParams& params,
                        scoped_refptr<feedback::FeedbackData> feedback_data);

#if BUILDFLAG(IS_CHROMEOS)
  // Gets logs that aren't covered by FetchSystemInformation, but should be
  // included in the feedback report. These currently consist of the Intel Wi-Fi
  // debug logs (if they exist).
  void FetchExtraLogs(const FeedbackParams& params,
                      scoped_refptr<feedback::FeedbackData> feedback_data);
  // Receive binary log files fetched from debugd.
  void OnBinaryLogFilesFetched(
      const FeedbackParams& params,
      scoped_refptr<feedback::FeedbackData> feedback_data,
      base::RepeatingClosure barrier_closure_callback,
      feedback::BinaryLogFilesReader::BinaryLogsResponse binary_logs);
  void OnExtraLogsFetched(const FeedbackParams& params,
                          scoped_refptr<feedback::FeedbackData> feedback_data);
#endif  // BUILDFLAG(IS_CHROMEOS)

  raw_ptr<content::BrowserContext, AcrossTasksDanglingUntriaged>
      browser_context_;
  raw_ptr<FeedbackPrivateDelegate, AcrossTasksDanglingUntriaged> delegate_;
#if BUILDFLAG(IS_CHROMEOS)
  // Root file path for log files. It can be overwritten for testing purpose.
  base::FilePath log_file_root_{FILE_PATH_LITERAL("/var/log/")};
  feedback::BinaryLogFilesReader binary_log_files_reader_;
#endif  // BUILDFLAG(IS_CHROMEOS)
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_FEEDBACK_SERVICE_H_
