// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_MOCK_FEEDBACK_SERVICE_H_
#define EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_MOCK_FEEDBACK_SERVICE_H_

#include "base/memory/ref_counted.h"
#include "extensions/browser/api/feedback_private/feedback_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {

class MockFeedbackService : public FeedbackService {
 public:
  explicit MockFeedbackService(content::BrowserContext* browser_context);
  MockFeedbackService(content::BrowserContext* browser_context,
                      FeedbackPrivateDelegate* delegate);

  MOCK_METHOD(void,
              RedactThenSendFeedback,
              (const FeedbackParams&,
               scoped_refptr<feedback::FeedbackData>,
               SendFeedbackCallback),
              (override));

 private:
  friend class base::RefCountedThreadSafe<MockFeedbackService>;
  ~MockFeedbackService() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_FEEDBACK_PRIVATE_MOCK_FEEDBACK_SERVICE_H_
