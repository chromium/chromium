// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/media/buffered_data_source_host_impl.h"

#include "media/base/timestamp_constants.h"

namespace blink {

// We want a relatively small window for estimating bandwidth,
// that way we don't need to worry too much about seeks and pause
// throwing off the estimates.
constexpr base::TimeDelta kDownloadHistoryWindowSeconds = base::Seconds(10.0);

// Limit the number of entries in the rate estimator queue.
// 1024 entries should be more than enough.
constexpr size_t kDownloadHistoryMaxEntries = 1024;

// Just in case someone gives progress one byte at a time,
// let's aggregate progress updates together until we reach
// at least this many bytes.
constexpr int64_t kDownloadHistoryMinBytesPerEntry = 1000;

BufferedDataSourceHostImpl::BufferedDataSourceHostImpl(
    base::RepeatingClosure progress_cb,
    const base::TickClock* tick_clock)
    : total_bytes_(0),
      did_loading_progress_(false),
      progress_cb_(std::move(progress_cb)),
      tick_clock_(tick_clock) {}

BufferedDataSourceHostImpl::~BufferedDataSourceHostImpl() = default;

void BufferedDataSourceHostImpl::SetTotalBytes(int64_t total_bytes) {
  total_bytes_ = total_bytes;
}

int64_t BufferedDataSourceHostImpl::UnloadedBytesInInterval(
    const Interval<int64_t>& interval) const {
  int64_t bytes = 0;
  auto i = buffered_byte_ranges_.find(interval.begin);
  while (i != buffered_byte_ranges_.end()) {
    if (i.interval_begin() >= interval.end)
      break;
    if (!i.value()) {
      Interval<int64_t> intersection = i.interval().Intersect(interval);
      if (!intersection.Empty())
        bytes += intersection.end - intersection.begin;
    }
    ++i;
  }
  return bytes;
}

void BufferedDataSourceHostImpl::AddBufferedByteRange(int64_t start,
                                                      int64_t end) {
  int64_t new_bytes = UnloadedBytesInInterval(Interval<int64_t>(start, end));
  if (new_bytes > 0)
    did_loading_progress_ = true;
  buffered_byte_ranges_.SetInterval(start, end, 1);

  base::TimeTicks now = tick_clock_->NowTicks();
  int64_t bytes_so_far = 0;
  if (!download_history_.empty())
    bytes_so_far = download_history_.back().second;
  bytes_so_far += new_bytes;

  // If the difference between the last entry and the second to last entry is
  // less than kDownloadHistoryMinBytesPerEntry, just overwrite the last entry.
  if (download_history_.size() > 1 &&
      download_history_.back().second - (download_history_.end() - 2)->second <
          kDownloadHistoryMinBytesPerEntry) {
    download_history_.back() = std::make_pair(now, bytes_so_far);
  } else {
    download_history_.emplace_back(now, bytes_so_far);
  }
  DCHECK_GE(download_history_.size(), 1u);
  // Drop entries that are too old.
  while (download_history_.size() > kDownloadHistoryMaxEntries ||
         download_history_.back().first - download_history_.front().first >
             kDownloadHistoryWindowSeconds) {
    download_history_.pop_front();
  }
  progress_cb_.Run();
}

static base::TimeDelta TimeForByteOffset(int64_t byte_offset,
                                         int64_t total_bytes,
                                         base::TimeDelta duration) {
  double position = static_cast<double>(byte_offset) / total_bytes;
  // Snap to the beginning/end where the approximation can look especially bad.
  if (position < 0.01)
    return base::TimeDelta();
  if (position > 0.99)
    return duration;
  return base::Milliseconds(
      static_cast<int64_t>(position * duration.InMilliseconds()));
}

void BufferedDataSourceHostImpl::AddBufferedTimeRanges(
    media::Ranges<base::TimeDelta>* buffered_time_ranges,
    base::TimeDelta media_duration) const {
  DCHECK(media_duration != media::kNoTimestamp);
  DCHECK(media_duration != media::kInfiniteDuration);
  if (total_bytes_ && !buffered_byte_ranges_.empty()) {
    for (const auto i : buffered_byte_ranges_) {
      if (i.second) {
        int64_t start = i.first.begin;
        int64_t end = i.first.end;
        buffered_time_ranges->Add(
            TimeForByteOffset(start, total_bytes_, media_duration),
            TimeForByteOffset(end, total_bytes_, media_duration));
      }
    }
  }
}

bool BufferedDataSourceHostImpl::DidLoadingProgress() {
  bool ret = did_loading_progress_;
  did_loading_progress_ = false;
  return ret;
}

double BufferedDataSourceHostImpl::DownloadRate() const {
  // If the download history is really small, any estimate we make is going to
  // be wildly inaccurate, so let's not make any estimates until we have more
  // data.
  if (download_history_.size() < 5)
    return 0.0;

  // The data we get is bursty, so we get multiple measuring points very close
  // together. These bursts will often lead us to over-estimate the download
  // rate. By iterating over the beginning of the time series and picking the
  // data point that has the lowest download rate, we avoid over-estimating.
  const double kVeryLargeRate = 1.0E20;
  double download_rate = kVeryLargeRate;
  for (size_t i = 0; i < std::min<size_t>(20, download_history_.size() - 3);
       i++) {
    int64_t downloaded_bytes =
        download_history_.back().second - download_history_[i].second;
    base::TimeTicks now = tick_clock_->NowTicks();
    base::TimeDelta download_time = now - download_history_[i].first;
    if (download_time <= base::TimeDelta())
      continue;
    download_rate =
        std::min(download_rate, downloaded_bytes / download_time.InSecondsF());
  }
  return download_rate == kVeryLargeRate ? 0.0 : download_rate;
}

bool BufferedDataSourceHostImpl::CanPlayThrough(
    base::TimeDelta current_position,
    base::TimeDelta media_duration,
    double playback_rate) const {
  DCHECK_GE(playback_rate, 0);
  if (!total_bytes_ || media_duration <= base::TimeDelta() ||
      media_duration == media::kInfiniteDuration) {
    return false;
  }
  if (current_position > media_duration)
    return true;

  const int64_t byte_pos =
      std::max<int64_t>(total_bytes_ * (current_position / media_duration), 0);
  const int64_t unloaded_bytes =
      UnloadedBytesInInterval(Interval<int64_t>(byte_pos, total_bytes_));
  if (unloaded_bytes == 0)
    return true;

  double download_rate = DownloadRate();
  return (download_rate > 0) &&
         ((unloaded_bytes / download_rate) <
          ((media_duration - current_position).InSecondsF() / playback_rate));
}

void BufferedDataSourceHostImpl::SetTickClockForTest(
    const base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
}

}  // namespace blink
