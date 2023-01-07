// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TEST_UTILITY_INPUT_BUILDER_H_
#define MEDIA_CAST_TEST_UTILITY_INPUT_BUILDER_H_

#include <limits.h>

#include <string>

namespace media {
namespace cast {
namespace test {

// This class handles general user input to the application. The user will be
// displayed with the title string and be given a default value. When forced
// a range, the input values should be within low_range to high_range.
// Setting low and high to INT_MIN/INT_MAX is equivalent to not setting a range.
class InputBuilder {
 public:
  InputBuilder(const std::string& title,
               const std::string& default_value,
               int low_range,
               int high_range);
  virtual ~InputBuilder();

  // Ask the user for input, reads input from the input source and returns
  // the answer. This method will keep asking the user until a correct answer
  // is returned and is thereby guaranteed to return a response that is
  // acceptable within the predefined range.
  // Input will be returned in either string or int format, base on the function
  // called.
  std::string GetStringInput() const;
  int GetIntInput() const;

 private:
  bool ValidateInput(const std::string& input) const;

  const std::string title_;
  const std::string default_value_;
  // Low and high range values for input validation.
  const int low_range_;
  const int high_range_;
};

}  // namespace test
}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TEST_UTILITY_INPUT_BUILDER_H_
