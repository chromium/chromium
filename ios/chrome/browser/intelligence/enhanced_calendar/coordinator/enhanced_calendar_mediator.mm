// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/enhanced_calendar/coordinator/enhanced_calendar_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/intelligence/enhanced_calendar/constants/error_strings.h"
#import "ios/chrome/browser/intelligence/enhanced_calendar/coordinator/enhanced_calendar_mediator_delegate.h"
#import "ios/chrome/browser/intelligence/enhanced_calendar/model/enhanced_calendar_configuration.h"
#import "ios/chrome/browser/intelligence/enhanced_calendar/model/enhanced_calendar_service_impl.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/add_to_calendar/add_to_calendar_api.h"
#import "ios/web/public/web_state.h"
#import "mojo/public/cpp/bindings/remote.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The string template to use for parsing the end and start date/time.
NSString* kDateTimeTemplate = @"dd/MM/yyyy HH:mm";

// String template to use for adding additional information to the calendar
// event summary.
constexpr std::string kCalendarEventSummaryAdditionalInfoTemplate = "\n{} {}";

// The string template to use for the calendar event summary.
constexpr std::string kCalendarEventSummaryTemplate = "{}\n";

// The string template to use for the calendar event title.
constexpr std::string kCalendarEventTitleTemplate = "{} {}";

}  // namespace

@implementation EnhancedCalendarMediator {
  // The config object holding everything needed to complete an Enhanced
  // Calendar model request, and which will hold the parsed response values to
  // present the final "add to calendar" UI.
  EnhancedCalendarConfiguration* _enhancedCalendarConfig;

  // Remote used to make calls to functions related to
  // `EnhancedCalendarService`.
  mojo::Remote<ai::mojom::EnhancedCalendarService> _enhancedCalendarService;

  // Instantiated to pipe virtual remote calls to overridden functions in
  // `EnhancedCalendarServiceImpl`.
  std::unique_ptr<ai::EnhancedCalendarServiceImpl> _enhancedCalendarServiceImpl;
}

- (instancetype)initWithWebState:(web::WebState*)webState
          enhancedCalendarConfig:
              (EnhancedCalendarConfiguration*)enhancedCalendarConfig {
  self = [super init];
  if (self) {
    _enhancedCalendarConfig = enhancedCalendarConfig;

    // Create the Enhanced Calendar service and bind it.
    mojo::PendingReceiver<ai::mojom::EnhancedCalendarService>
        enhancedCalendarReceiver =
            _enhancedCalendarService.BindNewPipeAndPassReceiver();
    _enhancedCalendarServiceImpl =
        std::make_unique<ai::EnhancedCalendarServiceImpl>(
            std::move(enhancedCalendarReceiver), webState);
  }
  return self;
}

- (void)disconnect {
  // Cancel any in-flight requests.
  _enhancedCalendarService.reset();
  _enhancedCalendarServiceImpl.reset();
}

- (void)startEnhancedCalendarRequest {
  // Create and set the request params.
  ai::mojom::EnhancedCalendarServiceRequestParamsPtr requestParams =
      ai::mojom::EnhancedCalendarServiceRequestParams::New();
  requestParams->selected_text =
      base::SysNSStringToUTF8(_enhancedCalendarConfig.selectedText);
  requestParams->surrounding_text =
      base::SysNSStringToUTF8(_enhancedCalendarConfig.surroundingText);

  // Response handling callback.
  __weak __typeof(self) weakSelf = self;
  base::OnceCallback<void(ai::mojom::EnhancedCalendarResponseResultPtr)>
      handleResponseCallback = base::BindOnce(
          ^void(ai::mojom::EnhancedCalendarResponseResultPtr result) {
            [weakSelf handleEnhancedCalendarResponseResult:std::move(result)];
          });

  // Execute the request.
  _enhancedCalendarService->ExecuteEnhancedCalendarRequest(
      std::move(requestParams), std::move(handleResponseCallback));
}

#pragma mark - EnhancedCalendarMutator

- (void)cancelEnhancedCalendarRequestAndDismissBottomSheet {
  [_delegate cancelRequestsAndDismissViewController:self];
}

#pragma mark - Private

// Handles the Enhanced Calendar response by passing it to the "add to calendar"
// integration provider. Overrides values in the config with the new
// model-provided values.
- (void)handleEnhancedCalendarResponseResult:
    (ai::mojom::EnhancedCalendarResponseResultPtr)responseResult {
  // Present the "add to calendar" UI with default values if the response is an
  // error.
  if (responseResult->is_error()) {
    std::string error = responseResult->get_error();

    // If the error is due to account change, dismiss the UI.
    if (error == ai::GetEnhancedCalendarErrorString(
                     ai::EnhancedCalendarError::kPrimaryAccountChangeError)) {
      [_delegate cancelRequestsAndDismissViewController:self];
      return;
    }

    [_delegate presentAddToCalendar:self config:_enhancedCalendarConfig];
    return;
  }

  optimization_guide::proto::EnhancedCalendarResponse enhancedCalendarResponse =
      responseResult->get_response()
          .As<optimization_guide::proto::EnhancedCalendarResponse>()
          .value();

  // Set the new values on the config.
  [self updateConfigWithEnhancedCalendarResponse:enhancedCalendarResponse];

  // Present the "add to calendar" UI.
  [_delegate presentAddToCalendar:self config:_enhancedCalendarConfig];
}

// Overrides the EnhancedCalendarConfiguration fields with the values from the
// EnhancedCalendarResponse provided by the model.
- (void)updateConfigWithEnhancedCalendarResponse:
    (optimization_guide::proto::EnhancedCalendarResponse)
        enhancedCalendarResponse {
  // Set the start date/time if one could be parsed, otherwise keep it default.
  NSDate* startDateTime =
      [self parseDateTimeWithDateString:enhancedCalendarResponse.start_date()
                             timeString:enhancedCalendarResponse.start_time()];
  if (startDateTime) {
    _enhancedCalendarConfig.calendarEventConfig.startDateTime = startDateTime;
  }

  // Set the end date/time if one could be parsed, otherwise keep it default.
  NSDate* endDateTime =
      [self parseDateTimeWithDateString:enhancedCalendarResponse.end_date()
                             timeString:enhancedCalendarResponse.end_time()];
  if (endDateTime) {
    _enhancedCalendarConfig.calendarEventConfig.endDateTime = endDateTime;
  }

  // The expected string template for the calendar event should be equivalent
  // to:
  // ```
  // {eventSummaryString}
  //
  // Location: {optional locationString}
  // URL: {URLString}
  // Confirmation code: {optional confirmationCode}
  // ```

  _enhancedCalendarConfig.calendarEventConfig.eventTitle = [self
      formattedCalendarEventTitle:enhancedCalendarResponse.event_title()
                    isEventBooked:enhancedCalendarResponse.is_event_booked()];
  _enhancedCalendarConfig.calendarEventConfig.eventDescription = [self
      formattedCalendarEventSummary:enhancedCalendarResponse.event_summary()
                      eventLocation:enhancedCalendarResponse.event_location()
                   confirmationCode:enhancedCalendarResponse
                                        .event_confirmation_code()];

  // Set `isAllDay`.
  _enhancedCalendarConfig.calendarEventConfig.isAllDay =
      enhancedCalendarResponse.is_all_day();

  // Set the recurrence.
  _enhancedCalendarConfig.calendarEventConfig.recurrence =
      enhancedCalendarResponse.recurrence();
}

// Parses an NSDate* from the concatenated date and time strings, according to
// `kDateTimeTemplate`. Returns nil if it could not parse an NSDate*.
- (NSDate*)parseDateTimeWithDateString:(std::string)dateString
                            timeString:(std::string)timeString {
  NSDateFormatter* dateFormatter = [[NSDateFormatter alloc] init];
  [dateFormatter setDateFormat:kDateTimeTemplate];

  NSString* dateTimeString =
      [NSString stringWithFormat:@"%@ %@", base::SysUTF8ToNSString(dateString),
                                 base::SysUTF8ToNSString(timeString)];
  return [dateFormatter dateFromString:dateTimeString];
}

// Get optional location field.
- (NSString*)optionalLocationField:(std::string)eventLocation {
  if (eventLocation.empty()) {
    return @"";
  }

  return base::SysUTF8ToNSString(
      std::format(kCalendarEventSummaryAdditionalInfoTemplate,
                  l10n_util::GetStringUTF8(
                      IDS_IOS_ENHANCED_CALENDAR_EVENT_DESCRIPTION_LOCATION),
                  eventLocation));
}

// Get description URL field.
- (NSString*)descriptionURL {
  CHECK(!_enhancedCalendarConfig.URL.empty());

  return base::SysUTF8ToNSString(std::format(
      kCalendarEventSummaryAdditionalInfoTemplate,
      l10n_util::GetStringUTF8(IDS_IOS_ENHANCED_CALENDAR_EVENT_DESCRIPTION_URL),
      _enhancedCalendarConfig.URL));
}

// Get optional confirmation code field.
- (NSString*)optionalConfirmationCodeField:(std::string)confirmationCode {
  if (confirmationCode.empty()) {
    return @"";
  }

  return base::SysUTF8ToNSString(std::format(
      kCalendarEventSummaryAdditionalInfoTemplate,
      l10n_util::GetStringUTF8(
          IDS_IOS_ENHANCED_CALENDAR_EVENT_DESCRIPTION_CONFIRMATION_CODE),
      confirmationCode));
}

// Get the event title.
- (NSString*)formattedCalendarEventTitle:(std::string)eventTitle
                           isEventBooked:(BOOL)isEventBooked {
  // Add an optional `BOOKED` prefix before the title of the event.
  if (!isEventBooked) {
    return base::SysUTF8ToNSString(eventTitle);
  }

  std::string prefix = l10n_util::GetStringUTF8(
      IDS_IOS_ENHANCED_CALENDAR_EVENT_TITLE_BOOKED_PREFIX);
  return base::SysUTF8ToNSString(
      std::format(kCalendarEventTitleTemplate, prefix, eventTitle));
}

// Get the event summary.
- (NSString*)formattedCalendarEventSummary:(std::string)eventSummary
                             eventLocation:(std::string)eventLocation
                          confirmationCode:(std::string)confirmationCode {
  // Set the templated description.
  NSString* summary = base::SysUTF8ToNSString(
      std::format(kCalendarEventSummaryTemplate, eventSummary));

  summary = [summary
      stringByAppendingString:[self optionalLocationField:eventLocation]];
  summary = [summary stringByAppendingString:
                         [self optionalConfirmationCodeField:confirmationCode]];
  summary = [summary stringByAppendingString:[self descriptionURL]];

  return summary;
}

@end
