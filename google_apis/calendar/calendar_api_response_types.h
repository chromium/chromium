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
#include "base/time/time.h"
#include "url/gurl.h"

namespace base {
class Value;
template <class StructType>
class JSONValueConverter;
}  // namespace base

namespace google_apis {

namespace calendar {

// Parses a calendar list item from the response.
class SingleCalendar {
 public:
  SingleCalendar();
  SingleCalendar(const SingleCalendar&);
  SingleCalendar& operator=(const SingleCalendar&);
  ~SingleCalendar();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<SingleCalendar>* converter);

  // The Calendar ID
  const std::string& id() const { return id_; }
  void set_id(const std::string& id) { id_ = id; }

  // The Calendar summary (i.e. name of the calendar shown in Google Calendar)
  const std::string& summary() const { return summary_; }
  void set_summary(const std::string& summary) { summary_ = summary; }

  // The color ID of the calendar
  const std::string& color_id() const { return color_id_; }
  void set_color_id(const std::string& color_id) { color_id_ = color_id; }

  // Indicates whether or not the calendar is selected.
  bool selected() const { return selected_; }
  void set_selected(bool selected) { selected_ = selected; }

  // Indicates whether or not the calendar is the primary calendar.
  bool primary() const { return primary_; }
  void set_primary(bool primary) { primary_ = primary; }

  // Return the approximate size of this calendar list item in bytes.
  int GetApproximateSizeInBytes() const;

 private:
  std::string id_;
  std::string summary_;
  std::string color_id_;
  bool selected_ = false;
  bool primary_ = false;
};

// Parses a list of calendars.
class CalendarList {
 public:
  CalendarList();
  CalendarList(const CalendarList&) = delete;
  CalendarList& operator=(const CalendarList&) = delete;
  ~CalendarList();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<CalendarList>* converter);

  // Creates CalendarList from parsed JSON.
  static std::unique_ptr<CalendarList> CreateFrom(const base::Value& value);

  // Returns ETag for this calendar list.
  const std::string& etag() const { return etag_; }

  // Returns the kind.
  const std::string& kind() const { return kind_; }

  void set_etag(const std::string& etag) { etag_ = etag; }
  void set_kind(const std::string& kind) { kind_ = kind; }

  // Returns a set of calendars.
  const std::vector<std::unique_ptr<SingleCalendar>>& items() const {
    return items_;
  }
  std::vector<std::unique_ptr<SingleCalendar>>* mutable_items() {
    return &items_;
  }

  void InjectItemForTesting(std::unique_ptr<SingleCalendar> item);

 private:
  std::string etag_;
  std::string kind_;

  std::vector<std::unique_ptr<SingleCalendar>> items_;
};

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

// Parses the attachment field in the `CalendarEvent` response.
class Attachment {
 public:
  Attachment();
  Attachment(const Attachment&);
  Attachment& operator=(const Attachment&);
  Attachment(Attachment&&) noexcept;
  Attachment& operator=(Attachment&&) noexcept;
  ~Attachment();

  // The title of the attachment (file name).
  const std::string& title() const { return title_; }
  void set_title(const std::string& title) { title_ = title; }

  // The URL of the attachment.
  const GURL& file_url() const { return file_url_; }
  void set_file_url(const GURL& file_url) { file_url_ = file_url; }

  // The URL of the attachment icon.
  const GURL& icon_link() const { return icon_link_; }
  void set_icon_link(const GURL& icon_link) { icon_link_ = icon_link; }

  // The file ID of the attachment.
  const std::string& file_id() const { return file_id_; }
  void set_file_id(const std::string& file_id) { file_id_ = file_id; }

 private:
  std::string title_;
  GURL file_url_;
  GURL icon_link_;
  std::string file_id_;
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

  // Boolean indicating if there is an attendee other than the user
  // attending the event. If the version of the request only fetches
  // 1 attendee, this will always be false.
  bool has_other_attendee() const { return has_other_attendee_; }
  void set_has_other_attendee(bool has_other_attendee) {
    has_other_attendee_ = has_other_attendee;
  }

  // The location of the event.
  const std::string& location() const { return location_; }
  void set_location(const std::string& location) { location_ = location; }

  // The start time of the event.
  const DateTime& start_time() const { return start_time_; }
  void set_start_time(const DateTime& start_time) { start_time_ = start_time; }

  // The end time of the event.
  const DateTime& end_time() const { return end_time_; }
  void set_end_time(const DateTime& end_time) { end_time_ = end_time; }

  // Boolean indicating if the event lasts all day.
  const bool& all_day_event() const { return all_day_event_; }
  void set_all_day_event(const bool& all_day_event) {
    all_day_event_ = all_day_event;
  }

  // Video conference URL, if one is attached to the event and using the
  // conferenceData.uri field. This could be 1P like Google Meet or any 3P
  // provider as long as they have integrated with the calendar API correctly.
  const GURL& conference_data_uri() const { return conference_data_uri_; }
  void set_conference_data_uri(const GURL& uri) { conference_data_uri_ = uri; }

  // The attachments of each event, if any.
  const std::vector<Attachment>& attachments() const { return attachments_; }
  void set_attachments(std::vector<Attachment> attachments) {
    attachments_ = std::move(attachments);
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
  bool has_other_attendee_ = false;
  std::string location_;
  DateTime start_time_;
  DateTime end_time_;
  bool all_day_event_ = false;
  GURL conference_data_uri_;
  std::vector<Attachment> attachments_;
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
