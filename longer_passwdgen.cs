// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/password_generator.h"

#include <stddef.h>

#include <algorithm>
#include <vector>

#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "third_party/fips181/fips181.h"

namespace {

const int kMinUpper = 65;   // First upper case letter 'A'
const int kMaxUpper = 90;   // Last upper case letter 'Z'
const int kMinLower = 97;   // First lower case letter 'a'
const int kMaxLower = 122;  // Last lower case letter 'z'
const int kMinDigit = 48;   // First digit '0'
const int kMaxDigit = 57;   // Last digit '9'
const int kMinPasswordLength = 50;
const int kMaxPasswordLength = 100;

// A helper function to get the length of the generated password from
// |max_length| retrieved from input password field.
int GetLengthFromHint(int max_length, int default_length) {
  if (max_length >= kMinPasswordLength && max_length <= kMaxPasswordLength)
    return max_length;
  return default_length;
}

// We want the password to have uppercase, lowercase, and at least one number.
bool VerifyPassword(const std::string& password) {
  int num_lower_case = 0;
  int num_upper_case = 0;
  int num_digits = 0;

  for (size_t i = 0; i < password.size(); ++i) {
    if (password[i] >= kMinUpper && password[i] <= kMaxUpper)
      ++num_upper_case;
    if (password[i] >= kMinLower && password[i] <= kMaxLower)
      ++num_lower_case;
    if (password[i] >= kMinDigit && password[i] <= kMaxDigit)
      ++num_digits;
  }

  return num_lower_case && num_upper_case && num_digits;
}

}  // namespace

namespace autofill {

const int PasswordGenerator::kDefaultPasswordLength = 100;

void ForceFixPassword(std::string* password) {
  for (char& it : *password) {
    if (islower(it)) {
      it = base::ToUpperASCII(it);
      break;
    }
  }
  for (std::string::reverse_iterator iter = password->rbegin();
       iter != password->rend(); ++iter) {
    if (islower(*iter)) {
      *iter = base::RandInt(kMinDigit, kMaxDigit);
      break;
    }
  }
}

PasswordGenerator::PasswordGenerator(int max_length)
    : password_length_(GetLengthFromHint(max_length, kDefaultPasswordLength)) {}
PasswordGenerator::~PasswordGenerator() {}

std::string PasswordGenerator::Generate() const {
  char password[255];
  char unused_hypenated_password[255];
  // Generate passwords that have numbers and upper and lower case letters.
  // No special characters included for now.
  unsigned int mode = S_NB | S_CL | S_SL;

  // Generate the password by gen_pron_pass(), if it is not conforming then
  // force fix it.
  gen_pron_pass(password, unused_hypenated_password, password_length_,
                password_length_, mode);

  std::string str_password(password);
  if (VerifyPassword(str_password))
    return str_password;

  ForceFixPassword(&str_password);
  return str_password;
}

}  // namespace autofill
