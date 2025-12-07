// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ENHANCED_CALENDAR_MODEL_ENHANCED_CALENDAR_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ENHANCED_CALENDAR_MODEL_ENHANCED_CALENDAR_CONFIGURATION_H_

#import <Foundation/Foundation.h>

#import "components/optimization_guide/proto/features/enhanced_calendar.pb.h"

namespace ios::provider {
enum class AddToCalendarIntegrationProvider;
}  // namespace ios::provider

// CalendarEventConfiguration is configuration class that holds all the data
// necessary for the creation of a simple calendar event using the "add to
// calendar" feature.
@interface CalendarEventConfiguration : NSObject

// The start date and time of the calendar event.
@property(nonatomic) NSDate* startDateTime;

// the end date and time of the calendar event.
@property(nonatomic) NSDate* endDateTime;

// The title of the calendar event.
@property(nonatomic, copy) NSString* eventTitle;

// The description of the calendar event.
@property(nonatomic, copy) NSString* eventDescription;

// Whether the calendar event is an all-day event.
@property(nonatomic) BOOL isAllDay;

// The currence state of the calendar event.
@property(nonatomic, assign)
    optimization_guide::proto::RecurrenceState recurrence;

@end

// EnhancedCalendarConfiguration is a configuration class that holds all the
// data necessary for an Enhanced Calendar event. It holds a
// CalendarEventConfiguration within it.
@interface EnhancedCalendarConfiguration : NSObject

// The calendar event configuration. This is the config that will be used to
// populate the "add to calendar" UI.
@property(nonatomic, strong) CalendarEventConfiguration* calendarEventConfig;

// The text selected by the user in the web view. This is the "event" in
// question.
@property(nonatomic, copy) NSString* selectedText;

// The text surrounding the selected text. This is used to provide context to
// uniquely identify the event.
@property(nonatomic, copy) NSString* surroundingText;

// The URL of the WebState where the user initiated the Enhanced Calendar
// feature.
@property(nonatomic, assign) std::string URL;

// The completion callback passed to the "add to calendar" UI.
@property(nonatomic, copy) void (^addToCalendarCompletion)(BOOL);

// The "add to calendar" integration provider that should be used.
@property(nonatomic, assign)
    ios::provider::AddToCalendarIntegrationProvider integrationProvider;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ENHANCED_CALENDAR_MODEL_ENHANCED_CALENDAR_CONFIGURATION_H_
