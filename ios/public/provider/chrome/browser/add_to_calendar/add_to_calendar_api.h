// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_ADD_TO_CALENDAR_ADD_TO_CALENDAR_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_ADD_TO_CALENDAR_ADD_TO_CALENDAR_API_H_

#import <UIKit/UIKit.h>

namespace web {
class WebState;
}  // namespace web

@class EnhancedCalendarConfiguration;

namespace ios::provider {

// The possible "add to calendar" integration providers.
enum class AddToCalendarIntegrationProvider {
  GOOGLE_CALENDAR_INTEGRATION,
  APPLE_CALENDAR_INTEGRATION,
};

// Configures and presents the "add to calendar" view using the
// `presenting_view_controller`. `config` holds all the calendar event data
// necessary to create and present the "add to calendar" UI.
void PresentAddToCalendar(UIViewController* presenting_view_controller,
                          web::WebState* web_state,
                          EnhancedCalendarConfiguration* config);

}  // namespace ios::provider

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_ADD_TO_CALENDAR_ADD_TO_CALENDAR_API_H_
