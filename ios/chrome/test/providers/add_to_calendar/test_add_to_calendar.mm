// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/add_to_calendar/add_to_calendar_api.h"

namespace ios::provider {

void PresentAddToCalendar(UIViewController* presenting_view_controller,
                          web::WebState* web_state,
                          EnhancedCalendarConfiguration* config) {}

// TODO(crbug.com/405195613): Cleanup function when provider migration is
// complete.
void PresentAddToCalendar(
    UIViewController* presenting_view_controller,
    AddToCalendarIntegrationProvider provider,
    base::WeakPtr<web::WebState> web_state,
    std::unique_ptr<optimization_guide::proto::EnhancedCalendarResponse>
        enhanced_calendar_response) {}

}  // namespace ios::provider
