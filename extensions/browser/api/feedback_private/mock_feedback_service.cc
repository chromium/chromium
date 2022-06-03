// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/feedback_private/mock_feedback_service.h"

namespace extensions {

MockFeedbackService::MockFeedbackService(
    content::BrowserContext* browser_context)
    : FeedbackService(browser_context) {}

MockFeedbackService::~MockFeedbackService() = default;

}  // namespace extensions
