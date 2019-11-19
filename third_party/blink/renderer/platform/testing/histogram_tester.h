// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_HISTOGRAM_TESTER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_HISTOGRAM_TESTER_H_

#include <memory>
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace base {
class HistogramTester;
}

namespace blink {

// Blink interface for base::HistogramTester.
class HistogramTester {
  USING_FAST_MALLOC(HistogramTester);

 public:
  HistogramTester();
  ~HistogramTester();

  void ExpectUniqueSample(const std::string& name,
                          base::HistogramBase::Sample,
                          base::HistogramBase::Count) const;
  void ExpectBucketCount(const std::string& name,
                         base::HistogramBase::Sample,
                         base::HistogramBase::Count) const;
  void ExpectTotalCount(const std::string& name,
                        base::HistogramBase::Count) const;
  base::HistogramBase::Count GetBucketCount(const std::string& name,
                                            base::HistogramBase::Sample) const;

 private:
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_HISTOGRAM_TESTER_H_
