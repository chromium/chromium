// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_CLASSROOM_CLASSROOM_API_STUDENTS_RESPONSE_TYPES_H_
#define GOOGLE_APIS_CLASSROOM_CLASSROOM_API_STUDENTS_RESPONSE_TYPES_H_

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

// Details of a user's name.
class Name {
 public:
  Name() = default;
  Name(const Name&) = delete;
  Name& operator=(const Name&) = delete;
  ~Name() = default;

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(base::JSONValueConverter<Name>* converter);

  const std::string& full_name() const { return full_name_; }

 private:
  // The user's full name.
  std::string full_name_;
};

// https://developers.google.com/classroom/reference/rest/v1/userProfiles
class UserProfile {
 public:
  UserProfile();
  UserProfile(const UserProfile&) = delete;
  UserProfile& operator=(const UserProfile&) = delete;
  ~UserProfile();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<UserProfile>* converter);

  const std::string& id() const { return id_; }
  const Name& name() const { return name_; }
  const std::string& email_address() const { return email_address_; }
  const GURL& photo_url() const { return photo_url_; }

 private:
  // Identifier of the user.
  std::string id_;

  // Name of the user.
  Name name_;

  // Email address of the user.
  std::string email_address_;

  // Photo url of the user.
  GURL photo_url_;
};

// https://developers.google.com/classroom/reference/rest/v1/courses.students
class Student {
 public:
  Student() = default;
  Student(const Student&) = delete;
  Student& operator=(const Student&) = delete;
  ~Student() = default;

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<Student>* converter);

  const UserProfile& profile() const { return profile_; }

 private:
  // Global user information for the student.
  UserProfile profile_;
};

// Container for multiple `Student`s.
class Students {
 public:
  Students();
  Students(const Students&) = delete;
  Students& operator=(const Students&) = delete;
  ~Students();

  // Registers the mapping between JSON field names and the members in this
  // class.
  static void RegisterJSONConverter(
      base::JSONValueConverter<Students>* converter);

  // Creates a `Students` from parsed JSON.
  static std::unique_ptr<Students> CreateFrom(const base::Value& value);

  const std::string& next_page_token() const { return next_page_token_; }
  const std::vector<std::unique_ptr<Student>>& items() const { return items_; }

 private:
  // `Student` items stored in this container.
  std::vector<std::unique_ptr<Student>> items_;

  // Token that can be used to request the next page of this result.
  std::string next_page_token_;
};

}  // namespace google_apis::classroom

#endif  // GOOGLE_APIS_CLASSROOM_CLASSROOM_API_STUDENTS_RESPONSE_TYPES_H_
