// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/link_to_text/model/link_to_text_utils.h"

#import "base/time/time.h"
#import "components/shared_highlighting/core/common/text_fragment.h"
#import "ios/chrome/browser/link_to_text/model/link_to_text_constants.h"

using shared_highlighting::LinkGenerationError;

namespace link_to_text {

std::optional<LinkGenerationOutcome> ParseStatus(std::optional<double> status) {
  if (!status.has_value()) {
    return std::nullopt;
  }

  int status_value = static_cast<int>(status.value());
  if (status_value < 0 ||
      status_value > static_cast<int>(LinkGenerationOutcome::kMaxValue)) {
    return std::nullopt;
  }

  return static_cast<LinkGenerationOutcome>(status_value);
}

shared_highlighting::LinkGenerationError OutcomeToError(
    LinkGenerationOutcome outcome) {
  switch (outcome) {
    case LinkGenerationOutcome::kInvalidSelection:
      return LinkGenerationError::kIncorrectSelector;
    case LinkGenerationOutcome::kAmbiguous:
      return LinkGenerationError::kContextExhausted;
    case LinkGenerationOutcome::kTimeout:
      return LinkGenerationError::kTimeout;
    case LinkGenerationOutcome::kExecutionFailed:
      return LinkGenerationError::kUnknown;
    case LinkGenerationOutcome::kSuccess:
      // kSuccess is not supposed to happen, as it is not an error.
      NOTREACHED_IN_MIGRATION();
      return LinkGenerationError::kUnknown;
  }
}

BOOL IsLinkGenerationTimeout(base::TimeDelta latency) {
  return latency >= kLinkGenerationTimeout;
}

}  // namespace link_to_text
