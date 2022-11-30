// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_RATE_LIMIT_RESULT_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_RATE_LIMIT_RESULT_H_

namespace content {

enum class RateLimitResult {
  kAllowed,
  kNotAllowed,
  kError,
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_RATE_LIMIT_RESULT_H_
