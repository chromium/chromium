// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/logging/stats_event_subscriber.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/format_macros.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"

#define STAT_ENUM_TO_STRING(enum) \
  case enum:                      \
    return #enum

namespace media {
namespace cast {

namespace {

using media::cast::CastLoggingEvent;
using media::cast::EventMediaType;

const size_t kMaxPacketEventTimeMapSize = 1000;

bool IsReceiverEvent(CastLoggingEvent event) {
  return event == FRAME_DECODED
      || event == FRAME_PLAYOUT
      || event == FRAME_ACK_SENT
      || event == PACKET_RECEIVED;
}

}  // namespace

StatsEventSubscriber::SimpleHistogram::SimpleHistogram(int64_t min,
                                                       int64_t max,
                                                       int64_t width)
    : min_(min), max_(max), width_(width), buckets_((max - min) / width + 2) {
  CHECK_GT(buckets_.size(), 2u);
  CHECK_EQ(0, (max_ - min_) % width_);
}

StatsEventSubscriber::SimpleHistogram::~SimpleHistogram() = default;

void StatsEventSubscriber::SimpleHistogram::Add(int64_t sample) {
  if (sample < min_) {
    ++buckets_.front();
  } else if (sample >= max_) {
    ++buckets_.back();
  } else {
    size_t index = 1 + (sample - min_) / width_;
    DCHECK_LT(index, buckets_.size());
    ++buckets_[index];
  }
}

void StatsEventSubscriber::SimpleHistogram::Reset() {
  buckets_.assign(buckets_.size(), 0);
}

base::Value::List StatsEventSubscriber::SimpleHistogram::GetHistogram() const {
  base::Value::List histo;

  if (buckets_.front()) {
    base::Value::Dict bucket;
    bucket.Set(base::StringPrintf("<%" PRId64, min_), buckets_.front());
    histo.Append(std::move(bucket));
  }

  for (size_t i = 1; i < buckets_.size() - 1; i++) {
    if (!buckets_[i])
      continue;
    base::Value::Dict bucket;
    int64_t lower = min_ + (i - 1) * width_;
    int64_t upper = lower + width_ - 1;
    bucket.Set(base::StringPrintf("%" PRId64 "-%" PRId64, lower, upper),
               buckets_[i]);
    histo.Append(std::move(bucket));
  }

  if (buckets_.back()) {
    base::Value::Dict bucket;
    bucket.Set(base::StringPrintf(">=%" PRId64, max_), buckets_.back());
    histo.Append(std::move(bucket));
  }
  return histo;
}

StatsEventSubscriber::StatsEventSubscriber(
    EventMediaType event_media_type,
    const base::TickClock* clock,
    ReceiverTimeOffsetEstimator* offset_estimator)
    : event_media_type_(event_media_type),
      clock_(clock),
      offset_estimator_(offset_estimator),
      capture_latency_datapoints_(0),
      encode_time_datapoints_(0),
      queueing_latency_datapoints_(0),
      network_latency_datapoints_(0),
      packet_latency_datapoints_(0),
      frame_latency_datapoints_(0),
      e2e_latency_datapoints_(0),
      num_frames_dropped_by_encoder_(0),
      num_frames_late_(0),
      start_time_(clock_->NowTicks()) {
  DCHECK(event_media_type == AUDIO_EVENT || event_media_type == VIDEO_EVENT);

  InitHistograms();
}

StatsEventSubscriber::~StatsEventSubscriber() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void StatsEventSubscriber::OnReceiveFrameEvent(const FrameEvent& frame_event) {
  DCHECK(thread_checker_.CalledOnValidThread());

  CastLoggingEvent type = frame_event.type;
  if (frame_event.media_type != event_media_type_)
    return;

  auto it = frame_stats_.find(type);
  if (it == frame_stats_.end()) {
    FrameLogStats stats;
    stats.event_counter = 1;
    stats.sum_size = frame_event.size;
    stats.sum_delay = frame_event.delay_delta;
    frame_stats_.insert(std::make_pair(type, stats));
  } else {
    ++(it->second.event_counter);
    it->second.sum_size += frame_event.size;
    it->second.sum_delay += frame_event.delay_delta;
  }

  bool is_receiver_event = IsReceiverEvent(type);
  UpdateFirstLastEventTime(frame_event.timestamp, is_receiver_event);

  if (type == FRAME_CAPTURE_BEGIN) {
    RecordFrameCaptureTime(frame_event);
  } else if (type == FRAME_CAPTURE_END) {
    RecordCaptureLatency(frame_event);
  } else if (type == FRAME_ENCODED) {
    RecordEncodeLatency(frame_event);
  } else if (type == FRAME_ACK_SENT) {
    RecordFrameTxLatency(frame_event);
  } else if (type == FRAME_PLAYOUT) {
    RecordE2ELatency(frame_event);
    base::TimeDelta delay_delta = frame_event.delay_delta;

    // Positive delay_delta means the frame is late.
    if (delay_delta.is_positive()) {
      num_frames_late_++;
      histograms_[LATE_FRAME_MS_HISTO]->Add(delay_delta.InMillisecondsF());
    }
  }

  if (is_receiver_event)
    UpdateLastResponseTime(frame_event.timestamp);
}

void StatsEventSubscriber::OnReceivePacketEvent(
    const PacketEvent& packet_event) {
  DCHECK(thread_checker_.CalledOnValidThread());

  CastLoggingEvent type = packet_event.type;
  if (packet_event.media_type != event_media_type_)
    return;

  auto it = packet_stats_.find(type);
  if (it == packet_stats_.end()) {
    PacketLogStats stats;
    stats.event_counter = 1;
    stats.sum_size = packet_event.size;
    packet_stats_.insert(std::make_pair(type, stats));
  } else {
    ++(it->second.event_counter);
    it->second.sum_size += packet_event.size;
  }

  bool is_receiver_event = IsReceiverEvent(type);
  UpdateFirstLastEventTime(packet_event.timestamp, is_receiver_event);

  if (type == PACKET_SENT_TO_NETWORK ||
      type == PACKET_RECEIVED) {
    RecordPacketRelatedLatencies(packet_event);
  } else if (type == PACKET_RETRANSMITTED) {
    // We only measure network latency using packets that doesn't have to be
    // retransmitted as there is precisely one sent-receive timestamp pairs.
    ErasePacketSentTime(packet_event);
  }

  if (is_receiver_event)
    UpdateLastResponseTime(packet_event.timestamp);
}

void StatsEventSubscriber::UpdateFirstLastEventTime(base::TimeTicks timestamp,
                                                    bool is_receiver_event) {
  if (is_receiver_event) {
    base::TimeDelta receiver_offset;
    if (!GetReceiverOffset(&receiver_offset))
      return;
    timestamp -= receiver_offset;
  }

  if (first_event_time_.is_null()) {
    first_event_time_ = timestamp;
  } else {
    first_event_time_ = std::min(first_event_time_, timestamp);
  }
  if (last_event_time_.is_null()) {
    last_event_time_ = timestamp;
  } else {
    last_event_time_ = std::max(last_event_time_, timestamp);
  }
}

base::Value::Dict StatsEventSubscriber::GetStats() const {
  StatsMap stats_map;
  GetStatsInternal(&stats_map);
  base::Value::Dict ret;

  base::Value::Dict stats;
  for (StatsMap::const_iterator it = stats_map.begin(); it != stats_map.end();
       ++it) {
    // Round to 3 digits after the decimal point.
    stats.Set(CastStatToString(it->first), round(it->second * 1000.0) / 1000.0);
  }

  // Populate all histograms.
  for (auto it = histograms_.begin(); it != histograms_.end(); ++it) {
    stats.Set(CastStatToString(it->first), it->second->GetHistogram());
  }

  ret.Set(event_media_type_ == AUDIO_EVENT
              ? StatsEventSubscriber::kAudioStatsDictKey
              : StatsEventSubscriber::kVideoStatsDictKey,
          std::move(stats));

  return ret;
}

void StatsEventSubscriber::Reset() {
  DCHECK(thread_checker_.CalledOnValidThread());

  frame_stats_.clear();
  packet_stats_.clear();
  total_capture_latency_ = base::TimeDelta();
  capture_latency_datapoints_ = 0;
  total_encode_time_ = base::TimeDelta();
  encode_time_datapoints_ = 0;
  total_queueing_latency_ = base::TimeDelta();
  queueing_latency_datapoints_ = 0;
  total_network_latency_ = base::TimeDelta();
  network_latency_datapoints_ = 0;
  total_packet_latency_ = base::TimeDelta();
  packet_latency_datapoints_ = 0;
  total_frame_latency_ = base::TimeDelta();
  frame_latency_datapoints_ = 0;
  total_e2e_latency_ = base::TimeDelta();
  e2e_latency_datapoints_ = 0;
  num_frames_dropped_by_encoder_ = 0;
  num_frames_late_ = 0;
  recent_frame_infos_.clear();
  packet_sent_times_.clear();
  start_time_ = clock_->NowTicks();
  last_response_received_time_ = base::TimeTicks();
  for (auto it = histograms_.begin(); it != histograms_.end(); ++it) {
    it->second->Reset();
  }

  first_event_time_ = base::TimeTicks();
  last_event_time_ = base::TimeTicks();
}

// static
const char* StatsEventSubscriber::CastStatToString(CastStat stat) {
  switch (stat) {
    STAT_ENUM_TO_STRING(CAPTURE_FPS);
    STAT_ENUM_TO_STRING(ENCODE_FPS);
    STAT_ENUM_TO_STRING(DECODE_FPS);
    STAT_ENUM_TO_STRING(AVG_CAPTURE_LATENCY_MS);
    STAT_ENUM_TO_STRING(AVG_ENCODE_TIME_MS);
    STAT_ENUM_TO_STRING(AVG_QUEUEING_LATENCY_MS);
    STAT_ENUM_TO_STRING(AVG_NETWORK_LATENCY_MS);
    STAT_ENUM_TO_STRING(AVG_PACKET_LATENCY_MS);
    STAT_ENUM_TO_STRING(AVG_FRAME_LATENCY_MS);
    STAT_ENUM_TO_STRING(AVG_E2E_LATENCY_MS);
    STAT_ENUM_TO_STRING(ENCODE_KBPS);
    STAT_ENUM_TO_STRING(TRANSMISSION_KBPS);
    STAT_ENUM_TO_STRING(RETRANSMISSION_KBPS);
    STAT_ENUM_TO_STRING(MS_SINCE_LAST_RECEIVER_RESPONSE);
    STAT_ENUM_TO_STRING(NUM_FRAMES_CAPTURED);
    STAT_ENUM_TO_STRING(NUM_FRAMES_DROPPED_BY_ENCODER);
    STAT_ENUM_TO_STRING(NUM_FRAMES_LATE);
    STAT_ENUM_TO_STRING(NUM_PACKETS_SENT);
    STAT_ENUM_TO_STRING(NUM_PACKETS_RETRANSMITTED);
    STAT_ENUM_TO_STRING(NUM_PACKETS_RECEIVED);
    STAT_ENUM_TO_STRING(NUM_PACKETS_RTX_REJECTED);
    STAT_ENUM_TO_STRING(FIRST_EVENT_TIME_MS);
    STAT_ENUM_TO_STRING(LAST_EVENT_TIME_MS);
    STAT_ENUM_TO_STRING(CAPTURE_LATENCY_MS_HISTO);
    STAT_ENUM_TO_STRING(ENCODE_TIME_MS_HISTO);
    STAT_ENUM_TO_STRING(QUEUEING_LATENCY_MS_HISTO);
    STAT_ENUM_TO_STRING(NETWORK_LATENCY_MS_HISTO);
    STAT_ENUM_TO_STRING(PACKET_LATENCY_MS_HISTO);
    STAT_ENUM_TO_STRING(FRAME_LATENCY_MS_HISTO);
    STAT_ENUM_TO_STRING(E2E_LATENCY_MS_HISTO);
    STAT_ENUM_TO_STRING(LATE_FRAME_MS_HISTO);
    STAT_ENUM_TO_STRING(ENQUEUE_FPS);
    STAT_ENUM_TO_STRING(UNKNOWN_OPEN_SCREEN_STAT);
    STAT_ENUM_TO_STRING(UNKNOWN_OPEN_SCREEN_HISTO);
  }
  NOTREACHED();
}

const int kDefaultMaxLatencyBucketMs = 800;
const int kDefaultBucketWidthMs = 20;

// For small latency values.
const int kSmallMaxLatencyBucketMs = 100;
const int kSmallBucketWidthMs = 5;

// For large latency values.
const int kLargeMaxLatencyBucketMs = 1200;
const int kLargeBucketWidthMs = 50;

void StatsEventSubscriber::InitHistograms() {
  histograms_[E2E_LATENCY_MS_HISTO] = std::make_unique<SimpleHistogram>(
      0, kLargeMaxLatencyBucketMs, kLargeBucketWidthMs);
  histograms_[QUEUEING_LATENCY_MS_HISTO] = std::make_unique<SimpleHistogram>(
      0, kDefaultMaxLatencyBucketMs, kDefaultBucketWidthMs);
  histograms_[NETWORK_LATENCY_MS_HISTO] = std::make_unique<SimpleHistogram>(
      0, kDefaultMaxLatencyBucketMs, kDefaultBucketWidthMs);
  histograms_[PACKET_LATENCY_MS_HISTO] = std::make_unique<SimpleHistogram>(
      0, kDefaultMaxLatencyBucketMs, kDefaultBucketWidthMs);
  histograms_[FRAME_LATENCY_MS_HISTO] = std::make_unique<SimpleHistogram>(
      0, kDefaultMaxLatencyBucketMs, kDefaultBucketWidthMs);
  histograms_[LATE_FRAME_MS_HISTO] = std::make_unique<SimpleHistogram>(
      0, kDefaultMaxLatencyBucketMs, kDefaultBucketWidthMs);
  histograms_[CAPTURE_LATENCY_MS_HISTO] = std::make_unique<SimpleHistogram>(
      0, kSmallMaxLatencyBucketMs, kSmallBucketWidthMs);
  histograms_[ENCODE_TIME_MS_HISTO] = std::make_unique<SimpleHistogram>(
      0, kSmallMaxLatencyBucketMs, kSmallBucketWidthMs);
}

void StatsEventSubscriber::GetStatsInternal(StatsMap* stats_map) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  stats_map->clear();

  base::TimeTicks end_time = clock_->NowTicks();

  PopulateFpsStat(
      end_time, FRAME_CAPTURE_BEGIN, CAPTURE_FPS, stats_map);
  PopulateFpsStat(
      end_time, FRAME_ENCODED, ENCODE_FPS, stats_map);
  PopulateFpsStat(
      end_time, FRAME_DECODED, DECODE_FPS, stats_map);
  PopulateFrameBitrateStat(end_time, stats_map);
  PopulatePacketBitrateStat(end_time,
                            PACKET_SENT_TO_NETWORK,
                            TRANSMISSION_KBPS,
                            stats_map);
  PopulatePacketBitrateStat(end_time,
                            PACKET_RETRANSMITTED,
                            RETRANSMISSION_KBPS,
                            stats_map);
  PopulateFrameCountStat(FRAME_CAPTURE_END, NUM_FRAMES_CAPTURED, stats_map);
  PopulatePacketCountStat(PACKET_SENT_TO_NETWORK, NUM_PACKETS_SENT, stats_map);
  PopulatePacketCountStat(
      PACKET_RETRANSMITTED, NUM_PACKETS_RETRANSMITTED, stats_map);
  PopulatePacketCountStat(PACKET_RECEIVED, NUM_PACKETS_RECEIVED, stats_map);
  PopulatePacketCountStat(
      PACKET_RTX_REJECTED, NUM_PACKETS_RTX_REJECTED, stats_map);

  if (capture_latency_datapoints_ > 0) {
    double avg_capture_latency_ms =
        total_capture_latency_.InMillisecondsF() / capture_latency_datapoints_;
    stats_map->insert(
        std::make_pair(AVG_CAPTURE_LATENCY_MS, avg_capture_latency_ms));
  }

  if (encode_time_datapoints_ > 0) {
    double avg_encode_time_ms =
        total_encode_time_.InMillisecondsF() / encode_time_datapoints_;
    stats_map->insert(
        std::make_pair(AVG_ENCODE_TIME_MS, avg_encode_time_ms));
  }

  if (queueing_latency_datapoints_ > 0) {
    double avg_queueing_latency_ms =
        total_queueing_latency_.InMillisecondsF() /
        queueing_latency_datapoints_;
    stats_map->insert(
        std::make_pair(AVG_QUEUEING_LATENCY_MS, avg_queueing_latency_ms));
  }

  if (network_latency_datapoints_ > 0) {
    double avg_network_latency_ms =
        total_network_latency_.InMillisecondsF() / network_latency_datapoints_;
    stats_map->insert(
        std::make_pair(AVG_NETWORK_LATENCY_MS, avg_network_latency_ms));
  }

  if (packet_latency_datapoints_ > 0) {
    double avg_packet_latency_ms =
        total_packet_latency_.InMillisecondsF() / packet_latency_datapoints_;
    stats_map->insert(
        std::make_pair(AVG_PACKET_LATENCY_MS, avg_packet_latency_ms));
  }

  if (frame_latency_datapoints_ > 0) {
    double avg_frame_latency_ms =
        total_frame_latency_.InMillisecondsF() / frame_latency_datapoints_;
    stats_map->insert(
        std::make_pair(AVG_FRAME_LATENCY_MS, avg_frame_latency_ms));
  }

  if (e2e_latency_datapoints_ > 0) {
    double avg_e2e_latency_ms =
        total_e2e_latency_.InMillisecondsF() / e2e_latency_datapoints_;
    stats_map->insert(std::make_pair(AVG_E2E_LATENCY_MS, avg_e2e_latency_ms));
  }

  if (!last_response_received_time_.is_null()) {
    stats_map->insert(
        std::make_pair(MS_SINCE_LAST_RECEIVER_RESPONSE,
        (end_time - last_response_received_time_).InMillisecondsF()));
  }

  stats_map->insert(std::make_pair(NUM_FRAMES_DROPPED_BY_ENCODER,
                                   num_frames_dropped_by_encoder_));
  stats_map->insert(std::make_pair(NUM_FRAMES_LATE, num_frames_late_));
  if (!first_event_time_.is_null()) {
    stats_map->insert(std::make_pair(
        FIRST_EVENT_TIME_MS,
        (first_event_time_ - base::TimeTicks::UnixEpoch()).InMillisecondsF()));
  }
  if (!last_event_time_.is_null()) {
    stats_map->insert(std::make_pair(
        LAST_EVENT_TIME_MS,
        (last_event_time_ - base::TimeTicks::UnixEpoch()).InMillisecondsF()));
  }
}

StatsEventSubscriber::SimpleHistogram*
StatsEventSubscriber::GetHistogramForTesting(
    CastStat stats) const {
  DCHECK(histograms_.find(stats) != histograms_.end());
  return histograms_.find(stats)->second.get();
}

bool StatsEventSubscriber::GetReceiverOffset(base::TimeDelta* offset) {
  base::TimeDelta receiver_offset_lower_bound;
  base::TimeDelta receiver_offset_upper_bound;
  if (!offset_estimator_->GetReceiverOffsetBounds(
          &receiver_offset_lower_bound, &receiver_offset_upper_bound)) {
    return false;
  }

  *offset = (receiver_offset_lower_bound + receiver_offset_upper_bound) / 2;
  return true;
}

void StatsEventSubscriber::MaybeInsertFrameInfo(RtpTimeTicks rtp_timestamp,
                                                const FrameInfo& frame_info) {
  // No need to insert if |rtp_timestamp| is the smaller than every key in the
  // map as it is just going to get erased anyway.
  if (recent_frame_infos_.size() == kMaxFrameInfoMapSize &&
      rtp_timestamp < recent_frame_infos_.begin()->first) {
    return;
  }

  recent_frame_infos_.insert(std::make_pair(rtp_timestamp, frame_info));

  if (recent_frame_infos_.size() >= kMaxFrameInfoMapSize) {
    auto erase_it = recent_frame_infos_.begin();
    if (erase_it->second.encode_end_time.is_null())
      num_frames_dropped_by_encoder_++;
    recent_frame_infos_.erase(erase_it);
  }
}

void StatsEventSubscriber::RecordFrameCaptureTime(
    const FrameEvent& frame_event) {
  FrameInfo frame_info;
  frame_info.capture_time = frame_event.timestamp;
  MaybeInsertFrameInfo(frame_event.rtp_timestamp, frame_info);
}

void StatsEventSubscriber::RecordCaptureLatency(const FrameEvent& frame_event) {
  auto it = recent_frame_infos_.find(frame_event.rtp_timestamp);
  if (it == recent_frame_infos_.end()) {
    return;
  }

  if (!it->second.capture_time.is_null()) {
    base::TimeDelta latency = frame_event.timestamp - it->second.capture_time;
    total_capture_latency_ += latency;
    capture_latency_datapoints_++;
    histograms_[CAPTURE_LATENCY_MS_HISTO]->Add(latency.InMillisecondsF());
  }

  it->second.capture_end_time = frame_event.timestamp;
}

void StatsEventSubscriber::RecordEncodeLatency(const FrameEvent& frame_event) {
  auto it = recent_frame_infos_.find(frame_event.rtp_timestamp);
  if (it == recent_frame_infos_.end()) {
    FrameInfo frame_info;
    frame_info.encode_end_time = frame_event.timestamp;
    MaybeInsertFrameInfo(frame_event.rtp_timestamp, frame_info);
    return;
  }

  if (!it->second.capture_end_time.is_null()) {
    base::TimeDelta latency =
        frame_event.timestamp - it->second.capture_end_time;
    total_encode_time_ += latency;
    encode_time_datapoints_++;
    histograms_[ENCODE_TIME_MS_HISTO]->Add(latency.InMillisecondsF());
  }

  it->second.encode_end_time = frame_event.timestamp;
}

void StatsEventSubscriber::RecordFrameTxLatency(const FrameEvent& frame_event) {
  auto it = recent_frame_infos_.find(frame_event.rtp_timestamp);
  if (it == recent_frame_infos_.end())
    return;

  if (it->second.encode_end_time.is_null())
    return;

  base::TimeDelta receiver_offset;
  if (!GetReceiverOffset(&receiver_offset))
    return;

  base::TimeTicks sender_time = frame_event.timestamp - receiver_offset;
  base::TimeDelta latency = sender_time - it->second.encode_end_time;
  total_frame_latency_ += latency;
  frame_latency_datapoints_++;
  histograms_[FRAME_LATENCY_MS_HISTO]->Add(latency.InMillisecondsF());
}

void StatsEventSubscriber::RecordE2ELatency(const FrameEvent& frame_event) {
  base::TimeDelta receiver_offset;
  if (!GetReceiverOffset(&receiver_offset))
    return;

  auto it = recent_frame_infos_.find(frame_event.rtp_timestamp);
  if (it == recent_frame_infos_.end())
    return;

  base::TimeTicks playout_time = frame_event.timestamp - receiver_offset;
  base::TimeDelta latency = playout_time - it->second.capture_time;
  total_e2e_latency_ += latency;
  e2e_latency_datapoints_++;
  histograms_[E2E_LATENCY_MS_HISTO]->Add(latency.InMillisecondsF());
}

void StatsEventSubscriber::UpdateLastResponseTime(
    base::TimeTicks receiver_time) {
  base::TimeDelta receiver_offset;
  if (!GetReceiverOffset(&receiver_offset))
    return;
  base::TimeTicks sender_time = receiver_time - receiver_offset;
  last_response_received_time_ = sender_time;
}

void StatsEventSubscriber::ErasePacketSentTime(
    const PacketEvent& packet_event) {
  std::pair<RtpTimeTicks, uint16_t> key(
      std::make_pair(packet_event.rtp_timestamp, packet_event.packet_id));
  packet_sent_times_.erase(key);
}

void StatsEventSubscriber::RecordPacketRelatedLatencies(
    const PacketEvent& packet_event) {
  // Log queueing latency.
  if (packet_event.type == PACKET_SENT_TO_NETWORK) {
    auto it = recent_frame_infos_.find(packet_event.rtp_timestamp);
    if (it != recent_frame_infos_.end()) {
      base::TimeDelta latency =
          packet_event.timestamp - it->second.encode_end_time;
      total_queueing_latency_ += latency;
      queueing_latency_datapoints_++;
      histograms_[QUEUEING_LATENCY_MS_HISTO]->Add(
          latency.InMillisecondsF());
    }
  }

  // Log network latency and total packet latency;
  base::TimeDelta receiver_offset;
  if (!GetReceiverOffset(&receiver_offset))
    return;

  std::pair<RtpTimeTicks, uint16_t> key(
      std::make_pair(packet_event.rtp_timestamp, packet_event.packet_id));
  auto it = packet_sent_times_.find(key);
  if (it == packet_sent_times_.end()) {
    std::pair<base::TimeTicks, CastLoggingEvent> value =
        std::make_pair(packet_event.timestamp, packet_event.type);
    packet_sent_times_.insert(std::make_pair(key, value));
    if (packet_sent_times_.size() > kMaxPacketEventTimeMapSize)
      packet_sent_times_.erase(packet_sent_times_.begin());
  } else {
    std::pair<base::TimeTicks, CastLoggingEvent> value = it->second;
    CastLoggingEvent recorded_type = value.second;
    bool match = false;
    base::TimeTicks packet_sent_time;
    base::TimeTicks packet_received_time;
    if (recorded_type == PACKET_SENT_TO_NETWORK &&
        packet_event.type == PACKET_RECEIVED) {
      packet_sent_time = value.first;
      packet_received_time = packet_event.timestamp;
      match = true;
    } else if (recorded_type == PACKET_RECEIVED &&
        packet_event.type == PACKET_SENT_TO_NETWORK) {
      packet_sent_time = packet_event.timestamp;
      packet_received_time = value.first;
      match = true;
    }
    if (match) {
      packet_sent_times_.erase(it);

      // Subtract by offset.
      packet_received_time -= receiver_offset;
      base::TimeDelta latency_delta = packet_received_time - packet_sent_time;

      total_network_latency_ += latency_delta;
      network_latency_datapoints_++;
      histograms_[NETWORK_LATENCY_MS_HISTO]->Add(
          latency_delta.InMillisecondsF());

      // Log total network latency.
      auto frame_it = recent_frame_infos_.find(packet_event.rtp_timestamp);
      if (frame_it != recent_frame_infos_.end()) {
        base::TimeDelta latency =
            packet_received_time - frame_it->second.encode_end_time;
        total_packet_latency_ += latency;
        packet_latency_datapoints_++;
        histograms_[PACKET_LATENCY_MS_HISTO]->Add(
            latency.InMillisecondsF());
      }
    }
  }
}

void StatsEventSubscriber::PopulateFpsStat(base::TimeTicks end_time,
                                           CastLoggingEvent event,
                                           CastStat stat,
                                           StatsMap* stats_map) const {
  auto it = frame_stats_.find(event);
  if (it != frame_stats_.end()) {
    double fps = 0.0;
    base::TimeDelta duration = (end_time - start_time_);
    int count = it->second.event_counter;
    if (duration.is_positive())
      fps = count / duration.InSecondsF();
    stats_map->insert(std::make_pair(stat, fps));
  }
}

void StatsEventSubscriber::PopulateFrameCountStat(CastLoggingEvent event,
                                                  CastStat stat,
                                                  StatsMap* stats_map) const {
  auto it = frame_stats_.find(event);
  if (it != frame_stats_.end()) {
    stats_map->insert(std::make_pair(stat, it->second.event_counter));
  }
}

void StatsEventSubscriber::PopulatePacketCountStat(CastLoggingEvent event,
                                                   CastStat stat,
                                                   StatsMap* stats_map) const {
  auto it = packet_stats_.find(event);
  if (it != packet_stats_.end()) {
    stats_map->insert(std::make_pair(stat, it->second.event_counter));
  }
}

void StatsEventSubscriber::PopulateFrameBitrateStat(base::TimeTicks end_time,
                                                    StatsMap* stats_map) const {
  auto it = frame_stats_.find(FRAME_ENCODED);
  if (it != frame_stats_.end()) {
    double kbps = 0.0;
    base::TimeDelta duration = end_time - start_time_;
    if (duration.is_positive()) {
      kbps = it->second.sum_size / duration.InMillisecondsF() * 8;
    }

    stats_map->insert(std::make_pair(ENCODE_KBPS, kbps));
  }
}

void StatsEventSubscriber::PopulatePacketBitrateStat(
    base::TimeTicks end_time,
    CastLoggingEvent event,
    CastStat stat,
    StatsMap* stats_map) const {
  auto it = packet_stats_.find(event);
  if (it != packet_stats_.end()) {
    double kbps = 0;
    base::TimeDelta duration = end_time - start_time_;
    if (duration.is_positive()) {
      kbps = it->second.sum_size / duration.InMillisecondsF() * 8;
    }

    stats_map->insert(std::make_pair(stat, kbps));
  }
}

StatsEventSubscriber::FrameLogStats::FrameLogStats()
    : event_counter(0), sum_size(0) {}
StatsEventSubscriber::FrameLogStats::~FrameLogStats() = default;

StatsEventSubscriber::PacketLogStats::PacketLogStats()
    : event_counter(0), sum_size(0) {}
StatsEventSubscriber::PacketLogStats::~PacketLogStats() = default;

StatsEventSubscriber::FrameInfo::FrameInfo() : encoded(false) {
}
StatsEventSubscriber::FrameInfo::~FrameInfo() = default;

}  // namespace cast
}  // namespace media
