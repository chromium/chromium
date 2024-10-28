// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef GOOGLE_APIS_PEOPLE_PEOPLE_API_REQUEST_TYPES_H_
#define GOOGLE_APIS_PEOPLE_PEOPLE_API_REQUEST_TYPES_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/values.h"

namespace google_apis::people {

// A person's email address.
// The API may silently reject values if the `value` field is not set.
//
// https://developers.google.com/people/api/rest/v1/people#Person.EmailAddress
struct EmailAddress {
  // Predefined values for `type`.
  static constexpr std::string_view kHomeType = "home";
  static constexpr std::string_view kWorkType = "work";
  static constexpr std::string_view kOtherType = "other";

  // The email address.
  std::string value;
  // The type of the email address. The type can be custom or one of the
  // predefined values above.
  std::string type;

  // Converts this struct to a dict. Requires an rvalue reference, and leaves
  // this struct in a valid but unspecified state.
  //
  // This should be called either with a moved struct, or an explicit copy of
  // one.
  base::Value::Dict ToDict() &&;
};

// From the People API reference:
//
// A person's name. If the name is a mononym, the family name is empty.
//
// https://developers.google.com/people/api/rest/v1/people#name
struct Name {
  // The family name.
  std::string family_name;
  // The given name.
  std::string given_name;

  // Converts this struct to a dict. Requires an rvalue reference, and leaves
  // this struct in a valid but unspecified state.
  //
  // This should be called either with a moved struct, or an explicit copy of
  // one.
  base::Value::Dict ToDict() &&;
};

// A person's phone number.
// The API may silently reject values if the `value` field is not set.
//
// https://developers.google.com/people/api/rest/v1/people#Person.EmailAddress
struct PhoneNumber {
  // Predefined values for `type`.
  static constexpr std::string_view kHomeType = "home";
  static constexpr std::string_view kWorkType = "work";
  static constexpr std::string_view kMobileType = "mobile";
  static constexpr std::string_view kHomeFaxType = "homeFax";
  static constexpr std::string_view kWorkFaxType = "workFax";
  static constexpr std::string_view kOtherFaxType = "otherFax";
  static constexpr std::string_view kPagerType = "pager";
  static constexpr std::string_view kWorkMobileType = "workMobile";
  static constexpr std::string_view kWorkPagerType = "workPager";
  static constexpr std::string_view kMainType = "main";
  static constexpr std::string_view kGoogleVoiceType = "googleVoice";
  static constexpr std::string_view kOtherType = "other";

  // The phone number.
  std::string value;
  // The type of the phone number. The type can be custom or one of the
  // predefined values above.
  std::string type;

  // Converts this struct to a dict. Requires an rvalue reference, and leaves
  // this struct in a valid but unspecified state.
  //
  // This should be called either with a moved struct, or an explicit copy of
  // one.
  base::Value::Dict ToDict() &&;
};

// A contact-based Person sent to mutation endpoints. Unlike the general
// `Person` struct, this struct ensures that any fields which should be a
// singleton for contact-based Persons - biographies, birthdays, genders, names
// - are not repeated.
//
// Documentation for a generic Person:
// https://developers.google.com/people/api/rest/v1/people#resource:-person
struct Contact {
  // The person's email addresses. For `people.connections.list` and
  // `otherContacts.list` the number of email addresses is limited to 100. If a
  // Person has more email addresses the entire set can be obtained by calling
  // `people.getBatchGet`.
  std::vector<EmailAddress> email_addresses;

  // The person's name.
  //
  // As this is a contact-based Person, this is enforced to be a singleton.
  // However, this will still be serialised into an array under the `names` key
  // as that is expected from the API.
  // This will be not be serialised if it is empty.
  Name name;

  // The person's phone numbers. For `people.connections.list` and
  // `otherContacts.list` the number of phone numbers is limited to 100. If a
  // Person has more phone numbers the entire set can be obtained by calling
  // `people.getBatchGet`.
  std::vector<PhoneNumber> phone_numbers;

  Contact();
  Contact(const Contact&);
  Contact& operator=(const Contact&);
  Contact(Contact&&);
  Contact& operator=(Contact&&);
  ~Contact();

  // Converts this struct to a dict. Requires an rvalue reference, and leaves
  // this struct in a valid but unspecified state.
  //
  // This should be called either with a moved struct, or an explicit copy of
  // one.
  base::Value::Dict ToDict() &&;
};

}  // namespace google_apis::people

#endif  // GOOGLE_APIS_PEOPLE_PEOPLE_API_REQUEST_TYPES_H_
