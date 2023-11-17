// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_OPTIMIZATION_GUIDE_TEST_UTILS_H_
#define IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_OPTIMIZATION_GUIDE_TEST_UTILS_H_

#include <string>

namespace base {
class HistogramTester;
}

// Retries fetching `histogram_name` until it contains at least `count` samples.
void RetryForHistogramUntilCountReached(
    const base::HistogramTester* histogram_tester,
    const std::string& histogram_name,
    int count);

#endif  // IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_OPTIMIZATION_GUIDE_TEST_UTILS_H_