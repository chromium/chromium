// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LINK_TO_TEXT_LINK_TO_TEXT_UTILS_H_
#define IOS_CHROME_BROWSER_LINK_TO_TEXT_LINK_TO_TEXT_UTILS_H_

#import <CoreGraphics/CoreGraphics.h>
#import <UIKit/UIKit.h>

#include <string>

#import "base/optional.h"
#import "components/shared_highlighting/core/common/shared_highlighting_metrics.h"
#import "ios/chrome/browser/link_to_text/link_generation_outcome.h"

namespace base {
class TimeDelta;
class Value;
}  // namespace base

namespace web {
class WebState;
}  // namespace web

namespace link_to_text {

// Returns whether |value| is a dictionary value, and is not empty.
BOOL IsValidDictValue(const base::Value* value);

// Attempts to convert a numerical |status| value from the
// text-fragments-polyfill library into a LinkGenerationOutcome enum
// value, representing outcomes for that library.
absl::optional<LinkGenerationOutcome> ParseStatus(
    absl::optional<double> status);

// Converts a given text-fragments-polyfill library error |outcome| to its
// LinkGenerationError counterpart.
shared_highlighting::LinkGenerationError OutcomeToError(
    LinkGenerationOutcome outcome);

// Attempts to parse the given |value| into a CGRect. If |value| does not map
// into the expected structure, an empty absl::optional instance will be
// returned.
absl::optional<CGRect> ParseRect(const base::Value* value);

// Attempts to parse the given |url_value| into a GURL instance. If |url_value|
// is empty or invalid, an empty absl::optional instance will be returned.
absl::optional<GURL> ParseURL(const std::string* url_value);

// Converts a given |web_view_rect| into its browser coordinates counterpart.
// Uses the given |web_state| to do the conversion.
CGRect ConvertToBrowserRect(CGRect web_view_rect, web::WebState* web_state);

// Returns YES if |latency| exceeds the timeout limit for link generation.
BOOL IsLinkGenerationTimeout(base::TimeDelta latency);

}  // namespace link_to_text

#endif  // IOS_CHROME_BROWSER_LINK_TO_TEXT_LINK_TO_TEXT_UTILS_H_
