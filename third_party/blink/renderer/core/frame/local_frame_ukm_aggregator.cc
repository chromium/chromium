// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/local_frame_ukm_aggregator.h"

#include "services/metrics/public/cpp/ukm_entry_builder.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/time.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

LocalFrameUkmAggregator::ScopedUkmHierarchicalTimer::ScopedUkmHierarchicalTimer(
    LocalFrameUkmAggregator* aggregator,
    size_t metric_index)
    : aggregator_(aggregator),
      metric_index_(metric_index),
      start_time_(CurrentTimeTicks()) {}

LocalFrameUkmAggregator::ScopedUkmHierarchicalTimer::ScopedUkmHierarchicalTimer(
    ScopedUkmHierarchicalTimer&& other)
    : aggregator_(other.aggregator_),
      metric_index_(other.metric_index_),
      start_time_(other.start_time_) {
  other.aggregator_ = nullptr;
}

LocalFrameUkmAggregator::ScopedUkmHierarchicalTimer::
    ~ScopedUkmHierarchicalTimer() {
  if (aggregator_ && base::TimeTicks::IsHighResolution()) {
    aggregator_->RecordSample(metric_index_, start_time_, CurrentTimeTicks());
  }
}

LocalFrameUkmAggregator::LocalFrameUkmAggregator(int64_t source_id,
                                                 ukm::UkmRecorder* recorder)
    : source_id_(source_id),
      recorder_(recorder),
      event_name_("Blink.UpdateTime"),
      event_frequency_(TimeDelta::FromSeconds(30)),
      last_flushed_time_(CurrentTimeTicks()) {
  // Record average and worst case for the primary metric.
  primary_metric_.worst_case_metric_name = "MainFrame.WorstCase";
  primary_metric_.average_metric_name = "MainFrame.Average";

  // Define the UMA for the primary metric.
  primary_metric_.uma_counter.reset(
      new CustomCountHistogram("Blink.MainFrame.UpdateTime", 0, 10000000, 50));

  // Set up the substrings to create the UMA names
  const String uma_preamble = "Blink.";
  const String uma_postscript = ".UpdateTime";
  const String uma_ratio_preamble = "Blink.MainFrame.";
  const String uma_ratio_postscript = "Ratio";

  // Set up sub-strings for the bucketed UMA metrics
  Vector<String> threshold_substrings;
  if (!bucket_thresholds().size()) {
    threshold_substrings.push_back(".All");
  } else {
    threshold_substrings.push_back(
        String::Format(".LessThan%lums",
                       (unsigned long)bucket_thresholds()[0].InMilliseconds()));
    for (wtf_size_t i = 1; i < bucket_thresholds().size(); ++i) {
      threshold_substrings.push_back(String::Format(
          ".%lumsTo%lums",
          (unsigned long)bucket_thresholds()[i - 1].InMilliseconds(),
          (unsigned long)bucket_thresholds()[i].InMilliseconds()));
    }
    threshold_substrings.push_back(String::Format(
        ".MoreThan%lums",
        (unsigned long)bucket_thresholds()[bucket_thresholds().size() - 1]
            .InMilliseconds()));
  }

  // Populate all the sub-metrics.
  absolute_metric_records_.ReserveInitialCapacity(kCount);
  ratio_metric_records_.ReserveInitialCapacity(kCount);
  for (unsigned i = 0; i < (unsigned)kCount; ++i) {
    const auto& metric_name = metric_strings()[i];

    // Absolute records report the absolute time for each metric, both
    // average and worst case. They have an associated UMA too that we
    // own and allocate here.
    auto& absolute_record = absolute_metric_records_.emplace_back();
    absolute_record.worst_case_metric_name = metric_name;
    absolute_record.worst_case_metric_name.append(".WorstCase");
    absolute_record.average_metric_name = metric_name;
    absolute_record.average_metric_name.append(".Average");
    absolute_record.reset();
    auto uma_name = uma_preamble;
    uma_name.append(metric_name);
    uma_name.append(uma_postscript);
    absolute_record.uma_counter.reset(
        new CustomCountHistogram(uma_name.Utf8().data(), 0, 10000000, 50));

    // Ratio records report the ratio of each metric to the primary metric,
    // average and worst case. UMA counters are also associated with the
    // ratios and we allocate and own them here.
    auto& ratio_record = ratio_metric_records_.emplace_back();
    ratio_record.worst_case_metric_name = metric_name;
    ratio_record.worst_case_metric_name.append(".WorstCaseRatio");
    ratio_record.average_metric_name = metric_name;
    ratio_record.average_metric_name.append(".AverageRatio");
    ratio_record.reset();
    for (auto bucket_substring : threshold_substrings) {
      String uma_name = uma_ratio_preamble;
      uma_name.append(metric_name);
      uma_name.append(uma_ratio_postscript);
      uma_name.append(bucket_substring);
      ratio_record.uma_counters_per_bucket.push_back(
          std::make_unique<CustomCountHistogram>(uma_name.Utf8().data(), 0,
                                                 10000000, 50));
    }
  }
}

LocalFrameUkmAggregator::~LocalFrameUkmAggregator() {
  Flush(TimeTicks());
}

LocalFrameUkmAggregator::ScopedUkmHierarchicalTimer
LocalFrameUkmAggregator::GetScopedTimer(size_t metric_index) {
  return ScopedUkmHierarchicalTimer(this, metric_index);
}

void LocalFrameUkmAggregator::RecordSample(size_t metric_index,
                                           TimeTicks start,
                                           TimeTicks end) {
  TimeDelta duration = end - start;

  // Append the duration to the appropriate metrics record.
  DCHECK_LT(metric_index, absolute_metric_records_.size());
  auto& record = absolute_metric_records_[metric_index];
  if (duration > record.worst_case_duration)
    record.worst_case_duration = duration;
  record.total_duration += duration;
  ++record.sample_count;

  // Record the UMA
  record.uma_counter->CountMicroseconds(duration);

  // Just record the duration for ratios. We compute the ratio later
  // when we know the frame time.
  ratio_metric_records_[metric_index].interval_duration += duration;
}

void LocalFrameUkmAggregator::RecordPrimarySample(TimeTicks start,
                                                  TimeTicks end) {
  FlushIfNeeded(end);

  TimeDelta duration = end - start;

  // Record UMA
  primary_metric_.uma_counter->CountMicroseconds(duration);

  if (duration.is_zero())
    return;

  // Record primary time information
  if (duration > primary_metric_.worst_case_duration)
    primary_metric_.worst_case_duration = duration;
  primary_metric_.total_duration += duration;
  ++primary_metric_.sample_count;

  // Compute all the dependent metrics, after finding which bucket we're in
  // for UMA data.
  size_t bucket_index = bucket_thresholds().size();
  for (size_t i = 0; i < bucket_index; ++i) {
    if (duration < bucket_thresholds()[i]) {
      bucket_index = i;
    }
  }

  for (auto& record : ratio_metric_records_) {
    double ratio =
        record.interval_duration.InMicrosecondsF() / duration.InMicrosecondsF();
    if (ratio > record.worst_case_ratio)
      record.worst_case_ratio = ratio;
    record.total_ratio += ratio;
    ++record.sample_count;
    record.uma_counters_per_bucket[bucket_index]->Count(floor(ratio * 100.0));
    record.interval_duration = TimeDelta();
  }

  has_data_ = true;
}

void LocalFrameUkmAggregator::FlushIfNeeded(TimeTicks current_time) {
  if (current_time >= last_flushed_time_ + event_frequency_)
    Flush(current_time);
}

void LocalFrameUkmAggregator::Flush(TimeTicks current_time) {
  last_flushed_time_ = current_time;
  if (!has_data_)
    return;
  DCHECK(primary_metric_.sample_count);

  ukm::UkmEntryBuilder builder(source_id_, event_name_.Utf8().data());
  builder.SetMetric(primary_metric_.worst_case_metric_name.Utf8().data(),
                    primary_metric_.worst_case_duration.InMicroseconds());
  builder.SetMetric(primary_metric_.average_metric_name.Utf8().data(),
                    primary_metric_.total_duration.InMicroseconds() /
                        static_cast<int64_t>(primary_metric_.sample_count));
  for (auto& record : absolute_metric_records_) {
    if (record.sample_count == 0)
      continue;
    builder.SetMetric(record.worst_case_metric_name.Utf8().data(),
                      record.worst_case_duration.InMicroseconds());
    builder.SetMetric(record.average_metric_name.Utf8().data(),
                      record.total_duration.InMicroseconds() /
                          static_cast<int64_t>(record.sample_count));
  }
  for (auto& record : ratio_metric_records_) {
    if (record.sample_count == 0)
      continue;
    builder.SetMetric(record.worst_case_metric_name.Utf8().data(),
                      record.worst_case_ratio);
    builder.SetMetric(
        record.average_metric_name.Utf8().data(),
        record.total_ratio / static_cast<float>(record.sample_count));
    record.reset();
  }
  builder.Record(recorder_);
  has_data_ = false;
}

}  // namespace blink
