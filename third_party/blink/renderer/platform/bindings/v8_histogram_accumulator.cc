// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/v8_histogram_accumulator.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

// static
V8HistogramAccumulator* V8HistogramAccumulator::GetInstance() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(V8HistogramAccumulator, histogram_accumulator,
                                  ());
  return &histogram_accumulator;
}

void* V8HistogramAccumulator::RegisterHistogram(base::HistogramBase* histogram,
                                                const std::string& name) {
  std::unique_ptr<HistogramAndSum> histogram_and_sum;
  if (name == "V8.CompileLazyMicroSeconds" ||
      name == "V8.CompileMicroSeconds" ||
      name == "V8.CompileEvalMicroSeconds" ||
      name == "V8.CompileSerializeMicroSeconds" ||
      name == "V8.CompileDeserializeMicroSeconds") {
    histogram_and_sum = std::make_unique<HistogramAndSum>(
        histogram, &compile_foreground_sum_microseconds_);
  } else if (name == "V8.CompileScriptMicroSeconds.BackgroundThread" ||
             name == "V8.CompileFunctionMicroSeconds.BackgroundThread" ||
             name == "V8.CompileDeserializeMicroSeconds.BackgroundThread") {
    histogram_and_sum = std::make_unique<HistogramAndSum>(
        histogram, &compile_background_sum_microseconds_);
  } else if (name == "V8.ExecuteMicroSeconds") {
    histogram_and_sum = std::make_unique<HistogramAndSum>(
        histogram, &execute_sum_microseconds_);
  } else {
    histogram_and_sum = std::make_unique<HistogramAndSum>(histogram);
  }
  // Several threads might call RegisterHistogram; protect the
  // histogram_and_sums_ data structure with a mutex. After that, calling
  // AddSample is thread safe, since we use atomic ints for counting.
  std::lock_guard<std::mutex> lock(histogram_and_sums_mutex_);
  histogram_and_sums_.emplace_back(std::move(histogram_and_sum));
  return histogram_and_sums_.back().get();
}

void V8HistogramAccumulator::AddSample(void* raw_histogram, int sample) {
  HistogramAndSum* histogram_and_sum =
      static_cast<HistogramAndSum*>(raw_histogram);
  histogram_and_sum->original_histogram->Add(sample);
  if (histogram_and_sum->sum_microseconds != nullptr) {
    *(histogram_and_sum->sum_microseconds) += sample;
  }
}

void V8HistogramAccumulator::GenerateDataInteractive() {
  compile_foreground_.interactive_histogram->AddTimeMicrosecondsGranularity(
      base::Microseconds(compile_foreground_sum_microseconds_.load()));
  compile_background_.interactive_histogram->AddTimeMicrosecondsGranularity(
      base::Microseconds(compile_background_sum_microseconds_.load()));
  execute_.interactive_histogram->AddTimeMicrosecondsGranularity(
      base::Microseconds(execute_sum_microseconds_.load()));
}
V8HistogramAccumulator::V8HistogramAccumulator() {
  // Create accumulating histograms.
  int min = 0;
  int max = 5 * 60 * 1000000;  // 5 min
  uint32_t buckets = 100;
  compile_foreground_.interactive_histogram = base::Histogram::FactoryGet(
      "V8.CompileForegroundMicroSeconds.Cumulative.Interactive", min, max,
      buckets, base::Histogram::kUmaTargetedHistogramFlag);

  compile_background_.interactive_histogram = base::Histogram::FactoryGet(
      "V8.CompileBackgroundMicroSeconds.Cumulative.Interactive", min, max,
      buckets, base::Histogram::kUmaTargetedHistogramFlag);

  execute_.interactive_histogram = base::Histogram::FactoryGet(
      "V8.ExecuteMicroSeconds.Cumulative.Interactive", min, max, buckets,
      base::Histogram::kUmaTargetedHistogramFlag);
}

}  // namespace blink
