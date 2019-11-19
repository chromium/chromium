// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_CONTENT_VERIFIER_SCOPED_UMA_RECORDER_H_
#define EXTENSIONS_BROWSER_CONTENT_VERIFIER_SCOPED_UMA_RECORDER_H_

#include "base/metrics/histogram_macros.h"

namespace extensions {

// Helper to record UMA for ComputedHashes::Reader::InitFromFile and results of
// initializing verified_contents.json file.
// Records failure UMA if RecordSuccess() isn't explicitly called.
template <const char* RESULT_HISTOGRAM_NAME, const char* TIME_HISTOGRAM_NAME>
class ScopedUMARecorder {
 public:
  ScopedUMARecorder() = default;

  ~ScopedUMARecorder() {
    if (recorded_)
      return;
    RecordImpl(false);
  }

  void RecordSuccess() {
    recorded_ = true;
    RecordImpl(true);
  }

 private:
  void RecordImpl(bool succeeded) {
    UMA_HISTOGRAM_BOOLEAN(RESULT_HISTOGRAM_NAME, succeeded);
    if (succeeded)
      UMA_HISTOGRAM_TIMES(TIME_HISTOGRAM_NAME, timer_.Elapsed());
  }

  bool recorded_ = false;
  base::ElapsedTimer timer_;
  DISALLOW_COPY_AND_ASSIGN(ScopedUMARecorder);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_CONTENT_VERIFIER_SCOPED_UMA_RECORDER_H_
