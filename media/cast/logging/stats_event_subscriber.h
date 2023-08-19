// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_LOGGING_STATS_EVENT_SUBSCRIBER_H_
#define MEDIA_CAST_LOGGING_STATS_EVENT_SUBSCRIBER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/logging/raw_event_subscriber.h"
#include "media/cast/logging/receiver_time_offset_estimator.h"

namespace media {
namespace cast {

class StatsEventSubscriberTest;

// A RawEventSubscriber implementation that subscribes to events,
// and aggregates them into stats.
class StatsEventSubscriber final : public RawEventSubscriber {
 public:
  StatsEventSubscriber(EventMediaType event_media_type,
                       const base::TickClock* clock,
                       ReceiverTimeOffsetEstimator* offset_estimator);

  StatsEventSubscriber(const StatsEventSubscriber&) = delete;
  StatsEventSubscriber& operator=(const StatsEventSubscriber&) = delete;

  ~StatsEventSubscriber() final;

  // RawReventSubscriber implementations.
  void OnReceiveFrameEvent(const FrameEvent& frame_event) final;
  void OnReceivePacketEvent(const PacketEvent& packet_event) final;

  // Returns stats as a DictionaryValue. The dictionary contains one entry -
  // "audio" or "video" pointing to an inner dictionary.
  // The inner dictionary consists of string - double entries, where the string
  // describes the name of the stat, and the double describes
  // the value of the stat. See CastStat and StatsMap below.
  base::Value::Dict GetStats() const;

  // Resets stats in this object.
  void Reset();

  static constexpr char kAudioStatsDictKey[] = "audio";
  static constexpr char kVideoStatsDictKey[] = "video";

  enum CastStat {
    // Capture frame rate.
    CAPTURE_FPS,
    // Encode frame rate.
    ENCODE_FPS,
    // Decode frame rate.
    DECODE_FPS,
    // Average capture latency in milliseconds.
    AVG_CAPTURE_LATENCY_MS,
    // Average encode duration in milliseconds.
    AVG_ENCODE_TIME_MS,
    // Duration from when a frame is encoded to when the packet is first
    // sent.
    AVG_QUEUEING_LATENCY_MS,
    // Duration from when a packet is transmitted to when it is received.
    // This measures latency from sender to receiver.
    AVG_NETWORK_LATENCY_MS,
    // Duration from when a frame is encoded to when the packet is first
    // received.
    AVG_PACKET_LATENCY_MS,
    // Average latency between frame encoded and the moment when the frame
    // is fully received.
    AVG_FRAME_LATENCY_MS,
    // Duration from when a frame is captured to when it should be played out.
    AVG_E2E_LATENCY_MS,
    // Encode bitrate in kbps.
    ENCODE_KBPS,
    // Packet transmission bitrate in kbps.
    TRANSMISSION_KBPS,
    // Packet retransmission bitrate in kbps.
    RETRANSMISSION_KBPS,
    // Duration in milliseconds since last receiver response.
    MS_SINCE_LAST_RECEIVER_RESPONSE,
    // Number of frames captured.
    NUM_FRAMES_CAPTURED,
    // Number of frames dropped by encoder.
    NUM_FRAMES_DROPPED_BY_ENCODER,
    // Number of late frames.
    NUM_FRAMES_LATE,
    // Number of packets that were sent (not retransmitted).
    NUM_PACKETS_SENT,
    // Number of packets that were retransmitted.
    NUM_PACKETS_RETRANSMITTED,
    // Number of packets that were received by receiver.
    NUM_PACKETS_RECEIVED,
    // Number of packets that had their retransmission cancelled.
    NUM_PACKETS_RTX_REJECTED,
    // Unix time in milliseconds of first event since reset.
    FIRST_EVENT_TIME_MS,
    // Unix time in milliseconds of last event since reset.
    LAST_EVENT_TIME_MS,

    // Histograms
    CAPTURE_LATENCY_MS_HISTO,
    ENCODE_TIME_MS_HISTO,
    QUEUEING_LATENCY_MS_HISTO,
    NETWORK_LATENCY_MS_HISTO,
    PACKET_LATENCY_MS_HISTO,
    FRAME_LATENCY_MS_HISTO,
    E2E_LATENCY_MS_HISTO,
    LATE_FRAME_MS_HISTO,

    // Frame enqueuing rate.
    ENQUEUE_FPS,
    // Enum to handle an unknown Openscreen stat that is not yet implemented in
    // Chrome.
    UNKNOWN_OPEN_SCREEN_STAT,
    // Enum to handle an unknown Openscreen histogram that is not yet
    // implemented in Chrome.
    UNKNOWN_OPEN_SCREEN_HISTO
  };

  static const char* CastStatToString(CastStat stat);

 private:
  // TODO(b/268543775): Replace friend class declarations with public getters
  // for tests.
  friend class StatsEventSubscriberTest;
  FRIEND_TEST_ALL_PREFIXES(StatsEventSubscriberTest, EmptyStats);
  FRIEND_TEST_ALL_PREFIXES(StatsEventSubscriberTest, CaptureEncode);
  FRIEND_TEST_ALL_PREFIXES(StatsEventSubscriberTest, Encode);
  FRIEND_TEST_ALL_PREFIXES(StatsEventSubscriberTest, Decode);
  FRIEND_TEST_ALL_PREFIXES(StatsEventSubscriberTest, PlayoutDelay);
  FRIEND_TEST_ALL_PREFIXES(StatsEventSubscriberTest, E2ELatency);
  FRIEND_TEST_ALL_PREFIXES(StatsEventSubscriberTest, Packets);
  FRIEND_TEST_ALL_PREFIXES(StatsEventSubscriberTest, Histograms);

  static const size_t kMaxFrameInfoMapSize = 100;

  // Generic statistics given the raw data. More specific data (e.g. frame rate
  // and bit rate) can be computed given the basic metrics.
  // Some of the metrics will only be set when applicable, e.g. delay and size.
  struct FrameLogStats {
    FrameLogStats();
    ~FrameLogStats();
    int event_counter;
    size_t sum_size;
    base::TimeDelta sum_delay;
  };

  struct PacketLogStats {
    PacketLogStats();
    ~PacketLogStats();
    int event_counter;
    size_t sum_size;
  };

  class SimpleHistogram {
   public:
    // This will create N+2 buckets where N = (max - min) / width:
    // Underflow bucket: < min
    // Bucket 0: [min, min + width - 1]
    // Bucket 1: [min + width, min + 2 * width - 1]
    // ...
    // Bucket N-1: [max - width, max - 1]
    // Overflow bucket: >= max
    // |min| must be less than |max|.
    // |width| must divide |max - min| evenly.
    SimpleHistogram(int64_t min, int64_t max, int64_t width);

    ~SimpleHistogram();

    void Add(int64_t sample);

    void Reset();

    base::Value::List GetHistogram() const;

   private:
    int64_t min_;
    int64_t max_;
    int64_t width_;
    std::vector<int> buckets_;
  };

  struct FrameInfo {
    FrameInfo();
    ~FrameInfo();

    base::TimeTicks capture_time;
    base::TimeTicks capture_end_time;
    base::TimeTicks encode_end_time;
    bool encoded;
  };

  typedef std::map<CastStat, double> StatsMap;
  typedef std::map<CastStat, std::unique_ptr<SimpleHistogram>> HistogramMap;
  typedef std::map<RtpTimeTicks, FrameInfo> FrameInfoMap;
  typedef std::map<std::pair<RtpTimeTicks, uint16_t>,
                   std::pair<base::TimeTicks, CastLoggingEvent>>
      PacketEventTimeMap;
  typedef std::map<CastLoggingEvent, FrameLogStats> FrameStatsMap;
  typedef std::map<CastLoggingEvent, PacketLogStats> PacketStatsMap;

  void InitHistograms();

  // Assigns |stats_map| with stats data. Used for testing.
  void GetStatsInternal(StatsMap* stats_map) const;

  // Return a histogram of the type specified.
  SimpleHistogram* GetHistogramForTesting(CastStat stats) const;

  void UpdateFirstLastEventTime(base::TimeTicks timestamp,
                                bool is_receiver_event);
  bool GetReceiverOffset(base::TimeDelta* offset);
  void MaybeInsertFrameInfo(RtpTimeTicks rtp_timestamp,
                            const FrameInfo& frame_info);
  void RecordFrameCaptureTime(const FrameEvent& frame_event);
  void RecordCaptureLatency(const FrameEvent& frame_event);
  void RecordEncodeLatency(const FrameEvent& frame_event);
  void RecordFrameTxLatency(const FrameEvent& frame_event);
  void RecordE2ELatency(const FrameEvent& frame_event);
  void RecordPacketSentTime(const PacketEvent& packet_event);
  void ErasePacketSentTime(const PacketEvent& packet_event);
  void RecordPacketRelatedLatencies(const PacketEvent& packet_event);
  void UpdateLastResponseTime(base::TimeTicks receiver_time);

  void PopulateFpsStat(base::TimeTicks now,
                       CastLoggingEvent event,
                       CastStat stat,
                       StatsMap* stats_map) const;
  void PopulateFrameCountStat(CastLoggingEvent event,
                              CastStat stat,
                              StatsMap* stats_map) const;
  void PopulatePacketCountStat(CastLoggingEvent event,
                               CastStat stat,
                               StatsMap* stats_map) const;
  void PopulateFrameBitrateStat(base::TimeTicks now, StatsMap* stats_map) const;
  void PopulatePacketBitrateStat(base::TimeTicks now,
                                 CastLoggingEvent event,
                                 CastStat stat,
                                 StatsMap* stats_map) const;

  const EventMediaType event_media_type_;

  // Not owned by this class.
  const raw_ptr<const base::TickClock> clock_;

  // Not owned by this class.
  const raw_ptr<ReceiverTimeOffsetEstimator> offset_estimator_;

  FrameStatsMap frame_stats_;
  PacketStatsMap packet_stats_;

  base::TimeDelta total_capture_latency_;
  int capture_latency_datapoints_;
  base::TimeDelta total_encode_time_;
  int encode_time_datapoints_;
  base::TimeDelta total_queueing_latency_;
  int queueing_latency_datapoints_;
  base::TimeDelta total_network_latency_;
  int network_latency_datapoints_;
  base::TimeDelta total_packet_latency_;
  int packet_latency_datapoints_;
  base::TimeDelta total_frame_latency_;
  int frame_latency_datapoints_;
  base::TimeDelta total_e2e_latency_;
  int e2e_latency_datapoints_;

  base::TimeTicks last_response_received_time_;

  int num_frames_dropped_by_encoder_;
  int num_frames_late_;

  // Fixed size map to record when recent frames were captured and other info.
  FrameInfoMap recent_frame_infos_;

  // Fixed size map to record when recent packets were sent.
  PacketEventTimeMap packet_sent_times_;

  // Sender time assigned on creation and |Reset()|.
  base::TimeTicks start_time_;
  base::TimeTicks first_event_time_;
  base::TimeTicks last_event_time_;

  HistogramMap histograms_;

  base::ThreadChecker thread_checker_;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_LOGGING_STATS_EVENT_SUBSCRIBER_H_
