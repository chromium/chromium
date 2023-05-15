// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_CLASSROOM_CLASSROOM_API_COURSES_RESPONSE_TYPES_H_
#define GOOGLE_APIS_CLASSROOM_CLASSROOM_API_COURSES_RESPONSE_TYPES_H_

#include <memory>
#include <string>
#include <vector>

namespace base {
template <class StructType>
class JSONValueConverter;
class Value;
}  // namespace base

namespace google_apis::classroom {

// https://developers.google.com/classroom/reference/rest/v1/courses
class Course {
 public:
  // State of the course.
  // There are more states can be returned by the API, but current users only
  // need "ACTIVE" courses.
  enum class State {
    kActive,
    kOther,
  };

  Course() = default;
  Course(const Course&) = delete;
  Course& operator=(const Course&) = delete;
  ~Course() = default;

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<Course>* converter);

  static std::string StateToString(State state);

  const std::string& id() const { return id_; }
  const std::string& name() const { return name_; }
  State state() const { return state_; }

 private:
  // Identifier for this course assigned by Classroom.
  std::string id_;

  // Name of the course. For example, "10th Grade Biology".
  std::string name_;

  // State of the course.
  State state_ = State::kOther;
};

// Container for multiple `Course`s.
class Courses {
 public:
  Courses();
  Courses(const Courses&) = delete;
  Courses& operator=(const Courses&) = delete;
  ~Courses();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<Courses>* converter);

  // Creates a `Courses` from parsed JSON.
  static std::unique_ptr<Courses> CreateFrom(const base::Value& value);

  const std::string& next_page_token() const { return next_page_token_; }
  const std::vector<std::unique_ptr<Course>>& items() const { return items_; }

 private:
  // `Course` items stored in this container.
  std::vector<std::unique_ptr<Course>> items_;

  // Token that can be used to request the next page of this result.
  std::string next_page_token_;
};

}  // namespace google_apis::classroom

#endif  // GOOGLE_APIS_CLASSROOM_CLASSROOM_API_COURSES_RESPONSE_TYPES_H_
