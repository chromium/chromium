// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_CALENDAR_CALENDAR_API_RESPONSE_TYPES_H_
#define GOOGLE_APIS_CALENDAR_CALENDAR_API_RESPONSE_TYPES_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace base {
class Value;
template <class StructType>
class JSONValueConverter;
}  // namespace base

namespace google_apis {

namespace calendar {

// Parses the time field in the calendar Events.list response.
class DateTime {
 public:
  DateTime();
  DateTime(const DateTime&);
  DateTime& operator=(const DateTime& src);
  ~DateTime();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<DateTime>* converter);

  // Creates DateTime from parsed JSON.
  static bool CreateDateTimeFromValue(const base::Value* value, DateTime* time);

  const base::Time& date_time() const { return date_time_; }
  void set_date_time(const base::Time& date_time) { date_time_ = date_time; }

 private:
  base::Time date_time_;
};

// Parses the event item from the response. Not every field is parsed. If you
// find the field you want to use is not parsed here, you will need to add it.
class CalendarEvent {
 public:
  CalendarEvent();
  CalendarEvent(const CalendarEvent&);
  CalendarEvent& operator=(const CalendarEvent&);
  ~CalendarEvent();

  // Status of the event.
  enum class EventStatus {
    kUnknown,
    kCancelled,
    kConfirmed,
    kTentative,
  };

  // The attendee's response status.
  enum class ResponseStatus {
    kUnknown,
    kAccepted,
    kDeclined,
    kNeedsAction,
    kTentative,
  };

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<CalendarEvent>* converter);

  // Creates CalendarEvent from parsed JSON.
  static std::unique_ptr<CalendarEvent> CreateFrom(const base::Value& value);

  // The ID of this Calendar Event.
  const std::string& id() const { return id_; }
  void set_id(const std::string& id) { id_ = id; }

  // The title of the event (meeting's name).
  const std::string& summary() const { return summary_; }
  void set_summary(const std::string& summary) { summary_ = summary; }

  // An absolute link to this event in the Google Calendar Web UI.
  const std::string& html_link() const { return html_link_; }
  void set_html_link(const std::string& link) { html_link_ = link; }

  // The color id of the event.
  const std::string& color_id() const { return color_id_; }
  void set_color_id(const std::string& color_id) { color_id_ = color_id; }

  // The status of the event.
  EventStatus status() const { return status_; }
  void set_status(EventStatus status) { status_ = status; }

  // The self attendency response status of the event.
  ResponseStatus self_response_status() const { return self_response_status_; }
  void set_self_response_status(ResponseStatus self_response_status) {
    self_response_status_ = self_response_status;
  }

  const DateTime& start_time() const { return start_time_; }
  void set_start_time(const DateTime& start_time) { start_time_ = start_time; }

  const DateTime& end_time() const { return end_time_; }
  void set_end_time(const DateTime& end_time) { end_time_ = end_time; }

  const bool& all_day_event() const { return all_day_event_; }
  void set_all_day_event(const bool& all_day_event) {
    all_day_event_ = all_day_event;
  }

  // Return the approximate size of this event, in bytes.
  int GetApproximateSizeInBytes() const;

 private:
  std::string id_;
  std::string summary_;
  std::string html_link_;
  std::string color_id_;
  EventStatus status_ = EventStatus::kUnknown;
  ResponseStatus self_response_status_ = ResponseStatus::kUnknown;
  DateTime start_time_;
  DateTime end_time_;
  bool all_day_event_;
};

// Parses a list of calendar events.
class EventList {
 public:
  EventList();
  EventList(const EventList&) = delete;
  EventList& operator=(const EventList&) = delete;
  ~EventList();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<EventList>* converter);

  // Creates EventList from parsed JSON.
  static std::unique_ptr<EventList> CreateFrom(const base::Value& value);

  // Returns time zone.
  const std::string& time_zone() const { return time_zone_; }

  // Returns ETag for this calendar.
  const std::string& etag() const { return etag_; }

  // Returns the kind.
  const std::string& kind() const { return kind_; }

  void set_time_zone(const std::string& time_zone) { time_zone_ = time_zone; }
  void set_etag(const std::string& etag) { etag_ = etag; }
  void set_kind(const std::string& kind) { kind_ = kind; }

  // Returns a set of events in this calendar.
  const std::vector<std::unique_ptr<CalendarEvent>>& items() const {
    return items_;
  }
  std::vector<std::unique_ptr<CalendarEvent>>* mutable_items() {
    return &items_;
  }

  void InjectItemForTesting(std::unique_ptr<CalendarEvent> item);

 private:
  std::string time_zone_;
  std::string etag_;
  std::string kind_;

  std::vector<std::unique_ptr<CalendarEvent>> items_;
};

}  // namespace calendar
}  // namespace google_apis

#endif  // GOOGLE_APIS_CALENDAR_CALENDAR_API_RESPONSE_TYPES_H_
