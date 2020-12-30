// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_MOCK_FEEDBACK_SERVICE_H_
#define EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_MOCK_FEEDBACK_SERVICE_H_

#include "extensions/browser/api/feedback_private/feedback_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {

class MockFeedbackService : public FeedbackService {
 public:
  explicit MockFeedbackService(content::BrowserContext* browser_context);
  virtual ~MockFeedbackService();

  MOCK_METHOD2(SendFeedback,
               void(scoped_refptr<feedback::FeedbackData>,
                    FeedbackService::SendFeedbackCallback));
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_MOCK_FEEDBACK_SERVICE_H_
