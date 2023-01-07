// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/image_annotation/image_annotation_utils.h"

#include <map>

#include "base/no_destructor.h"

namespace image_annotation {

DescFailureReason ParseDescFailureReason(const std::string& reason_string) {
  const static base::NoDestructor<std::map<std::string, DescFailureReason>>
      kFailureReasonStrings(
          {{"UNKNOWN", DescFailureReason::kUnknown},
           {"OTHER", DescFailureReason::kOther},
           {"POLICY_VIOLATION", DescFailureReason::kPolicyViolation},
           {"ADULT", DescFailureReason::kAdult}});

  const auto lookup = kFailureReasonStrings->find(reason_string);
  return lookup == kFailureReasonStrings->end() ? DescFailureReason::kUnknown
                                                : lookup->second;
}

}  // namespace image_annotation
