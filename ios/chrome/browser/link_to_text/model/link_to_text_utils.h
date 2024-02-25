// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LINK_TO_TEXT_MODEL_LINK_TO_TEXT_UTILS_H_
#define IOS_CHROME_BROWSER_LINK_TO_TEXT_MODEL_LINK_TO_TEXT_UTILS_H_

#import <optional>

#import "components/shared_highlighting/core/common/shared_highlighting_metrics.h"
#import "ios/chrome/browser/link_to_text/model/link_generation_outcome.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace link_to_text {

// Attempts to convert a numerical `status` value from the
// text-fragments-polyfill library into a LinkGenerationOutcome enum
// value, representing outcomes for that library.
std::optional<LinkGenerationOutcome> ParseStatus(std::optional<double> status);

// Converts a given text-fragments-polyfill library error `outcome` to its
// LinkGenerationError counterpart.
shared_highlighting::LinkGenerationError OutcomeToError(
    LinkGenerationOutcome outcome);

// Returns YES if `latency` exceeds the timeout limit for link generation.
BOOL IsLinkGenerationTimeout(base::TimeDelta latency);

}  // namespace link_to_text

#endif  // IOS_CHROME_BROWSER_LINK_TO_TEXT_MODEL_LINK_TO_TEXT_UTILS_H_
