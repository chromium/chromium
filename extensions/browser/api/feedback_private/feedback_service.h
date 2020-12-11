// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_FEEDBACK_SERVICE_H_
#define EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_FEEDBACK_SERVICE_H_

#include <stdint.h>

#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/feedback/feedback_data.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

// The feedback service provides the ability to gather the various pieces of
// data needed to send a feedback report and then send the report once all
// the pieces are available.
class FeedbackService : public base::SupportsWeakPtr<FeedbackService> {
 public:
  // Callback invoked when the feedback report is ready to be sent.
  // True will be passed to indicate that it is being successfully sent now,
  // and false to indicate that it will be delayed (usually due to being
  // offline).
  using SendFeedbackCallback = base::Callback<void(bool)>;

  explicit FeedbackService(content::BrowserContext* browser_context);
  virtual ~FeedbackService();

  // Sends a feedback report.
  virtual void SendFeedback(scoped_refptr<feedback::FeedbackData> feedback_data,
                            const SendFeedbackCallback& callback);

 private:
  // Callbacks to receive blob data.
  void AttachedFileCallback(scoped_refptr<feedback::FeedbackData> feedback_data,
                            const SendFeedbackCallback& callback,
                            std::unique_ptr<std::string> data,
                            int64_t total_blob_length);
  void ScreenshotCallback(scoped_refptr<feedback::FeedbackData> feedback_data,
                          const SendFeedbackCallback& callback,
                          std::unique_ptr<std::string> data,
                          int64_t total_blob_length);

  // Checks if we have read all the blobs we need to; signals the feedback
  // data object once all the requisite data has been populated.
  void CompleteSendFeedback(scoped_refptr<feedback::FeedbackData> feedback_data,
                            const SendFeedbackCallback& callback);

  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(FeedbackService);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_FEEDBACK_SERVICE_H_
