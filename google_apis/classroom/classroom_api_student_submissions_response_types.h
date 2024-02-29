// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_CLASSROOM_CLASSROOM_API_STUDENT_SUBMISSIONS_RESPONSE_TYPES_H_
#define GOOGLE_APIS_CLASSROOM_CLASSROOM_API_STUDENT_SUBMISSIONS_RESPONSE_TYPES_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"

namespace base {
template <class StructType>
class JSONValueConverter;
class Value;
}  // namespace base

namespace google_apis::classroom {

// https://developers.google.com/classroom/reference/rest/v1/courses.courseWork.studentSubmissions
class StudentSubmission {
 public:
  // State of the student submission.
  enum class State {
    kNew,
    kCreated,
    kTurnedIn,
    kReturned,
    kReclaimedByStudent,
    kOther,
  };

  StudentSubmission() = default;
  StudentSubmission(const StudentSubmission&) = delete;
  StudentSubmission& operator=(const StudentSubmission&) = delete;
  ~StudentSubmission() = default;

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<StudentSubmission>* converter);

  const std::string& course_work_id() const { return course_work_id_; }
  const std::string& id() const { return id_; }
  std::optional<double> assigned_grade() const { return assigned_grade_; }
  const std::optional<base::Time>& last_update() const { return last_update_; }
  State state() const { return state_; }

 private:
  // Identifier for the course work which this submission belongs to.
  std::string course_work_id_;

  // Identifier for this student submission assigned by Classroom.
  std::string id_;

  // Optional grade. If unset, no grade was set. This value is a
  // non-negative decimal rounded to two decimal places.
  std::optional<double> assigned_grade_ = std::nullopt;

  // The last update time of this submission. May be unset if student has not
  // accessed this item.
  std::optional<base::Time> last_update_ = std::nullopt;

  // State of the student submission.
  State state_ = State::kOther;
};

class StudentSubmissions {
 public:
  StudentSubmissions();
  StudentSubmissions(const StudentSubmissions&) = delete;
  StudentSubmissions& operator=(const StudentSubmissions&) = delete;
  ~StudentSubmissions();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<StudentSubmissions>* converter);

  // Creates a `StudentSubmissions` from parsed JSON.
  static std::unique_ptr<StudentSubmissions> CreateFrom(
      const base::Value& value);

  const std::string& next_page_token() const { return next_page_token_; }
  const std::vector<std::unique_ptr<StudentSubmission>>& items() const {
    return items_;
  }

 private:
  // `StudentSubmission` items stored in this container.
  std::vector<std::unique_ptr<StudentSubmission>> items_;

  // Token that can be used to request the next page of this result.
  std::string next_page_token_;
};

}  // namespace google_apis::classroom

#endif  // GOOGLE_APIS_CLASSROOM_CLASSROOM_API_STUDENT_SUBMISSIONS_RESPONSE_TYPES_H_
