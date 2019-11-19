// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IMAGE_ANNOTATION_IMAGE_ANNOTATION_UTILS_H_
#define SERVICES_IMAGE_ANNOTATION_IMAGE_ANNOTATION_UTILS_H_

#include <string>

namespace image_annotation {

// An enum to describe the modes of description engine failure.
//
// Logged in metrics - do not reuse or reassign values.
enum class DescFailureReason {
  kUnknown = 0,
  kOther = 1,
  kPolicyViolation = 2,
  kAdult = 3,
  kMaxValue = kAdult,
};

// Returns the DescFailureReason enum value that the given string represents, or
// |kUnknown| if none apply.
DescFailureReason ParseDescFailureReason(const std::string& reason_string);

}  // namespace image_annotation

#endif  // SERVICES_IMAGE_ANNOTATION_IMAGE_ANNOTATION_UTILS_H_
