// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_CLASSROOM_CLASSROOM_API_COURSE_WORK_RESPONSE_TYPES_H_
#define GOOGLE_APIS_CLASSROOM_CLASSROOM_API_COURSE_WORK_RESPONSE_TYPES_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "url/gurl.h"

namespace base {
template <class StructType>
class JSONValueConverter;
class Value;
}  // namespace base

namespace google_apis::classroom {

// https://developers.google.com/classroom/reference/rest/v1/courses.courseWork
class CourseWorkItem {
 public:
  // State of the course work item.
  // There are more states can be returned by the API, but current users only
  // need "PUBLISHED" course work items.
  enum class State {
    kPublished,
    kOther,
  };

  // Joined due date and due time of the course work item.
  struct DueDateTime {
    // [1,9999], or 0 to specify a date without a year.
    int year;

    // [1,12], or 0 to specify a year without a month and day.
    int month;

    // [1,31], or 0 to specify a year by itself or a year and month where the
    // day isn't significant.
    int day;

    // Due time of this course work item ([0-24h) interval relative to the
    // year/month/day above). Expected to be set if the date part is presented.
    base::TimeDelta time_of_day;
  };

  CourseWorkItem();
  CourseWorkItem(const CourseWorkItem&) = delete;
  CourseWorkItem& operator=(const CourseWorkItem&) = delete;
  ~CourseWorkItem();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<CourseWorkItem>* converter);

  // Custom conversion entrypoint. Needed to join `dueDate` and `dueTime` JSON
  // fields located at the root level into `due_date_time_`.
  static bool ConvertCourseWorkItem(const base::Value* input,
                                    CourseWorkItem* output);

  const std::string& id() const { return id_; }
  const std::string& title() const { return title_; }
  State state() const { return state_; }
  const GURL& alternate_link() const { return alternate_link_; }
  const std::optional<DueDateTime>& due_date_time() const {
    return due_date_time_;
  }
  const base::Time& creation_time() const { return creation_time_; }
  const base::Time& last_update() const { return last_update_; }

 private:
  // Classroom-assigned identifier of this course work, unique per course.
  std::string id_;

  // Title of this course work item.
  std::string title_;

  // Status of this course work item.
  State state_ = State::kOther;

  // Absolute link to this course work in the Classroom web UI.
  GURL alternate_link_;

  // Optional due date and time in UTC of this course work item.
  // There could be a `base::Time` instead (because Classroom web UI supports
  // concrete/exact dates only), but the API uses a data structure that allows
  // specifying zeroes in different date components (e.g. a month and day with
  // a zero year means a repeating annual assignment). That is why it was safer,
  // more flexible and forward compatible to use the same approach here.
  std::optional<DueDateTime> due_date_time_ = std::nullopt;

  // The timestamp when this course work was created.
  base::Time creation_time_;

  // The timestamp of the last course work item update.
  base::Time last_update_;
};

// Container for multiple `CourseWorkItem`s.
class CourseWork {
 public:
  CourseWork();
  CourseWork(const CourseWork&) = delete;
  CourseWork& operator=(const CourseWork&) = delete;
  ~CourseWork();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<CourseWork>* converter);

  // Creates a `CourseWork` from parsed JSON.
  static std::unique_ptr<CourseWork> CreateFrom(const base::Value& value);

  const std::string& next_page_token() const { return next_page_token_; }
  const std::vector<std::unique_ptr<CourseWorkItem>>& items() const {
    return items_;
  }

 private:
  // `CourseWorkItem` items stored in this container.
  std::vector<std::unique_ptr<CourseWorkItem>> items_;

  // Token that can be used to request the next page of this result.
  std::string next_page_token_;
};

}  // namespace google_apis::classroom

#endif  // GOOGLE_APIS_CLASSROOM_CLASSROOM_API_COURSE_WORK_RESPONSE_TYPES_H_
