// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/html_parser_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace blink {

HTMLParserMetrics::HTMLParserMetrics(int64_t source_id,
                                     ukm::UkmRecorder* recorder)
    : source_id_(source_id), recorder_(recorder) {}

void HTMLParserMetrics::AddChunk(base::TimeDelta elapsed_time,
                                 unsigned tokens_parsed) {
  DCHECK(base::TimeTicks::IsHighResolution());

  ++chunk_count_;

  accumulated_parsing_time_ += elapsed_time;
  if (elapsed_time < min_parsing_time_)
    min_parsing_time_ = elapsed_time;
  if (elapsed_time > max_parsing_time_)
    max_parsing_time_ = elapsed_time;

  total_tokens_parsed_ += tokens_parsed;
  if (tokens_parsed < min_tokens_parsed_)
    min_tokens_parsed_ = tokens_parsed;
  if (tokens_parsed > max_tokens_parsed_)
    max_tokens_parsed_ = tokens_parsed;
}

void HTMLParserMetrics::AddYieldInterval(base::TimeDelta elapsed_time) {
  DCHECK(base::TimeTicks::IsHighResolution());

  yield_count_++;

  accumulated_yield_intervals_ += elapsed_time;
  if (elapsed_time < min_yield_interval_)
    min_yield_interval_ = elapsed_time;
  if (elapsed_time > max_yield_interval_)
    max_yield_interval_ = elapsed_time;
}

void HTMLParserMetrics::AddInput(unsigned length) {
  input_character_count_ += length;
}

void HTMLParserMetrics::AddFetchQueuedPreloadsTime(int64_t elapsed_time) {
  fetch_queued_preloads_time_ += elapsed_time;
}

void HTMLParserMetrics::AddPreloadTime(int64_t elapsed_time) {
  preload_time_ += elapsed_time;
}

void HTMLParserMetrics::AddPrepareToStopParsingTime(int64_t elapsed_time) {
  prepare_to_stop_parsing_time_ += elapsed_time;
}

void HTMLParserMetrics::AddPumpTokenizerTime(int64_t elapsed_time) {
  pump_tokenizer_time_ += elapsed_time;
}

void HTMLParserMetrics::AddScanAndPreloadTime(int64_t elapsed_time) {
  scan_and_preload_time_ += elapsed_time;
}

void HTMLParserMetrics::AddScanTime(int64_t elapsed_time) {
  scan_time_ += elapsed_time;
}

void HTMLParserMetrics::ReportUMAs() {
  UMA_HISTOGRAM_COUNTS_1000("Blink.HTMLParsing.ChunkCount4", chunk_count_);
  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "Blink.HTMLParsing.ParsingTimeMax4", max_parsing_time_,
      base::Microseconds(1), base::Seconds(100), 1000);
  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "Blink.HTMLParsing.ParsingTimeMin4", min_parsing_time_,
      base::Microseconds(1), base::Seconds(1), 100);
  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "Blink.HTMLParsing.ParsingTimeTotal4", accumulated_parsing_time_,
      base::Microseconds(1), base::Seconds(100), 1000);
  UMA_HISTOGRAM_COUNTS_1M("Blink.HTMLParsing.TokensParsedMax4",
                          max_tokens_parsed_);
  UMA_HISTOGRAM_COUNTS_10000("Blink.HTMLParsing.TokensParsedMin4",
                             min_tokens_parsed_);
  UMA_HISTOGRAM_COUNTS_1M("Blink.HTMLParsing.TokensParsedAverage4",
                          total_tokens_parsed_ / chunk_count_);
  UMA_HISTOGRAM_COUNTS_10M("Blink.HTMLParsing.TokensParsedTotal4",
                           total_tokens_parsed_);
  UMA_HISTOGRAM_COUNTS_1000("Blink.HTMLParsing.PreloadRequestCount",
                            total_preload_request_count_);

  // Only report yield data if we actually yielded.
  if (max_yield_interval_ != base::TimeDelta()) {
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Blink.HTMLParsing.YieldedTimeMax4", max_yield_interval_,
        base::Microseconds(1), base::Seconds(100), 1000);
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Blink.HTMLParsing.YieldedTimeMin4", min_yield_interval_,
        base::Microseconds(1), base::Seconds(10), 100);
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Blink.HTMLParsing.YieldedTimeAverage4",
        accumulated_yield_intervals_ / yield_count_, base::Microseconds(1),
        base::Seconds(10), 100);
  }

  UMA_HISTOGRAM_COUNTS_10M("Blink.HTMLParsing.InputCharacterCount4",
                           input_character_count_);
}

void HTMLParserMetrics::ReportMetricsAtParseEnd() {
  if (!chunk_count_)
    return;

  ReportUMAs();

  // Build and report UKM
  ukm::builders::Blink_HTMLParsing builder(source_id_);
  builder.SetChunkCount(chunk_count_);
  builder.SetParsingTimeMax(max_parsing_time_.InMicroseconds());
  builder.SetParsingTimeMin(min_parsing_time_.InMicroseconds());
  builder.SetParsingTimeTotal(accumulated_parsing_time_.InMicroseconds());
  builder.SetTokensParsedMax(max_tokens_parsed_);
  builder.SetTokensParsedMin(min_tokens_parsed_);
  builder.SetTokensParsedAverage(total_tokens_parsed_ / chunk_count_);
  builder.SetTokensParsedTotal(total_tokens_parsed_);
  if (accumulated_yield_intervals_ != base::TimeDelta()) {
    builder.SetYieldedTimeMax(max_yield_interval_.InMicroseconds());
    builder.SetYieldedTimeMin(min_yield_interval_.InMicroseconds());
    builder.SetYieldedTimeAverage(
        accumulated_yield_intervals_.InMicroseconds() / yield_count_);
  }
  if (fetch_queued_preloads_time_ > 0 || preload_time_ > 0 ||
      prepare_to_stop_parsing_time_ > 0 || pump_tokenizer_time_ > 0 ||
      scan_time_ > 0 || scan_and_preload_time_ > 0) {
    builder.SetFetchQueuedPreloadsTime(
        ukm::GetExponentialBucketMinForUserTiming(fetch_queued_preloads_time_));
    builder.SetPreloadTime(
        ukm::GetExponentialBucketMinForUserTiming(preload_time_));
    builder.SetPrepareToStopParsingTime(
        ukm::GetExponentialBucketMinForUserTiming(
            prepare_to_stop_parsing_time_));
    builder.SetPumpTokenizerTime(
        ukm::GetExponentialBucketMinForUserTiming(pump_tokenizer_time_));
    builder.SetScanAndPreloadTime(
        ukm::GetExponentialBucketMinForUserTiming(scan_and_preload_time_));
    builder.SetScanTime(ukm::GetExponentialBucketMinForUserTiming(scan_time_));
  }
  builder.Record(recorder_);
}

}  // namespace blink
