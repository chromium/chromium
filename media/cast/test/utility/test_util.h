// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TEST_UTILITY_TEST_UTIL_H_
#define MEDIA_CAST_TEST_UTILITY_TEST_UTIL_H_

#include <stddef.h>

#include <string>
#include <vector>

namespace media {
namespace cast {
namespace test {

class MeanAndError {
 public:
  MeanAndError() {}
  explicit MeanAndError(const std::vector<double>& values);
  std::string AsString() const;

  size_t num_values;
  double mean;
  double std_dev;
};

}  // namespace test
}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TEST_UTILITY_TEST_UTIL_H_
