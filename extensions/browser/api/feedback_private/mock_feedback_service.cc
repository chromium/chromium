// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/feedback_private/mock_feedback_service.h"

namespace extensions {

MockFeedbackService::MockFeedbackService(
    content::BrowserContext* browser_context)
    : FeedbackService(browser_context) {}

MockFeedbackService::MockFeedbackService(
    content::BrowserContext* browser_context,
    FeedbackPrivateDelegate* delegate)
    : FeedbackService(browser_context, delegate) {}

MockFeedbackService::~MockFeedbackService() = default;

}  // namespace extensions
