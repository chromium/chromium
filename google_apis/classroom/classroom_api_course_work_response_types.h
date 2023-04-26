// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_CLASSROOM_CLASSROOM_API_COURSE_WORK_RESPONSE_TYPES_H_
#define GOOGLE_APIS_CLASSROOM_CLASSROOM_API_COURSE_WORK_RESPONSE_TYPES_H_

#include <memory>
#include <string>
#include <vector>

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

  CourseWorkItem();
  CourseWorkItem(const CourseWorkItem&) = delete;
  CourseWorkItem& operator=(const CourseWorkItem&) = delete;
  ~CourseWorkItem();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<CourseWorkItem>* converter);

  const std::string& id() const { return id_; }
  const std::string& title() const { return title_; }
  State state() const { return state_; }
  const GURL& alternate_link() const { return alternate_link_; }

 private:
  // Classroom-assigned identifier of this course work, unique per course.
  std::string id_;

  // Title of this course work item.
  std::string title_;

  // Status of this course work item.
  State state_ = State::kOther;

  // Absolute link to this course work in the Classroom web UI.
  GURL alternate_link_;
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
