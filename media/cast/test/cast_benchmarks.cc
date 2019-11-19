// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This program benchmarks the theoretical throughput of the cast library.
// It runs using a fake clock, simulated network and fake codecs. This allows
// tests to run much faster than real time.
// To run the program, run:
// $ ./out/Release/cast_benchmarks | tee benchmarkoutput.asc
// This may take a while, when it is done, you can view the data with
// meshlab by running:
// $ meshlab benchmarkoutput.asc
// After starting meshlab, turn on Render->Show Axis. The red axis will
// represent bandwidth (in megabits) the blue axis will be packet drop
// (in percent) and the green axis will be latency (in milliseconds).
//
// This program can also be used for profiling. On linux it has
// built-in support for this. Simply set the environment variable
// PROFILE_FILE before running it, like so:
// $ export PROFILE_FILE=cast_benchmark.profile
// Then after running the program, you can view the profile with:
// $ pprof ./out/Release/cast_benchmarks $PROFILE_FILE --gv

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/profiler.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/thread.h"
#include "base/time/tick_clock.h"
#include "media/base/audio_bus.h"
#include "media/base/fake_single_thread_task_runner.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/cast_receiver.h"
#include "media/cast/cast_sender.h"
#include "media/cast/logging/simple_event_subscriber.h"
#include "media/cast/net/cast_transport.h"
#include "media/cast/net/cast_transport_config.h"
#include "media/cast/net/cast_transport_defines.h"
#include "media/cast/net/cast_transport_impl.h"
#include "media/cast/test/loopback_transport.h"
#include "media/cast/test/skewed_single_thread_task_runner.h"
#include "media/cast/test/skewed_tick_clock.h"
#include "media/cast/test/utility/audio_utility.h"
#include "media/cast/test/utility/default_config.h"
#include "media/cast/test/utility/test_util.h"
#include "media/cast/test/utility/udp_proxy.h"
#include "media/cast/test/utility/video_utility.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {

namespace {

static const int64_t kStartMillisecond = INT64_C(1245);
static const int kTargetPlayoutDelayMs = 400;

void ExpectVideoSuccess(OperationalStatus status) {
  EXPECT_EQ(STATUS_INITIALIZED, status);
}

void ExpectAudioSuccess(OperationalStatus status) {
  EXPECT_EQ(STATUS_INITIALIZED, status);
}

}  // namespace

// Wraps a CastTransport and records some statistics about
// the data that goes through it.
class CastTransportWrapper : public CastTransport {
 public:
  // Takes ownership of |transport|.
  void Init(CastTransport* transport,
            uint64_t* encoded_video_bytes,
            uint64_t* encoded_audio_bytes) {
    transport_.reset(transport);
    encoded_video_bytes_ = encoded_video_bytes;
    encoded_audio_bytes_ = encoded_audio_bytes;
  }

  void InitializeStream(const CastTransportRtpConfig& config,
                        std::unique_ptr<RtcpObserver> rtcp_observer) final {
    if (config.rtp_payload_type <= RtpPayloadType::AUDIO_LAST)
      audio_ssrc_ = config.ssrc;
    else
      video_ssrc_ = config.ssrc;
    transport_->InitializeStream(config, std::move(rtcp_observer));
  }

  void InsertFrame(uint32_t ssrc, const EncodedFrame& frame) final {
    if (ssrc == audio_ssrc_) {
      *encoded_audio_bytes_ += frame.data.size();
    } else if (ssrc == video_ssrc_) {
      *encoded_video_bytes_ += frame.data.size();
    }
    transport_->InsertFrame(ssrc, frame);
  }

  void SendSenderReport(uint32_t ssrc,
                        base::TimeTicks current_time,
                        RtpTimeTicks current_time_as_rtp_timestamp) final {
    transport_->SendSenderReport(ssrc,
                                 current_time,
                                 current_time_as_rtp_timestamp);
  }

  void CancelSendingFrames(uint32_t ssrc,
                           const std::vector<FrameId>& frame_ids) final {
    transport_->CancelSendingFrames(ssrc, frame_ids);
  }

  void ResendFrameForKickstart(uint32_t ssrc, FrameId frame_id) final {
    transport_->ResendFrameForKickstart(ssrc, frame_id);
  }

  PacketReceiverCallback PacketReceiverForTesting() final {
    return transport_->PacketReceiverForTesting();
  }

  void AddValidRtpReceiver(uint32_t rtp_sender_ssrc,
                           uint32_t rtp_receiver_ssrc) final {
    return transport_->AddValidRtpReceiver(rtp_sender_ssrc, rtp_receiver_ssrc);
  }

  void InitializeRtpReceiverRtcpBuilder(uint32_t rtp_receiver_ssrc,
                                        const RtcpTimeData& time_data) final {
    transport_->InitializeRtpReceiverRtcpBuilder(rtp_receiver_ssrc, time_data);
  }

  void AddCastFeedback(const RtcpCastMessage& cast_message,
                       base::TimeDelta target_delay) final {
    transport_->AddCastFeedback(cast_message, target_delay);
  }

  void AddRtcpEvents(
      const ReceiverRtcpEventSubscriber::RtcpEvents& rtcp_events) final {
    transport_->AddRtcpEvents(rtcp_events);
  }

  void AddRtpReceiverReport(const RtcpReportBlock& rtp_report_block) final {
    transport_->AddRtpReceiverReport(rtp_report_block);
  }

  void AddPli(const RtcpPliMessage& pli_message) final {
    transport_->AddPli(pli_message);
  }

  void SendRtcpFromRtpReceiver() final {
    transport_->SendRtcpFromRtpReceiver();
  }

  void SetOptions(const base::DictionaryValue& options) final {}

 private:
  std::unique_ptr<CastTransport> transport_;
  uint32_t audio_ssrc_, video_ssrc_;
  uint64_t* encoded_video_bytes_;
  uint64_t* encoded_audio_bytes_;
};

struct MeasuringPoint {
  MeasuringPoint(double bitrate_, double latency_, double percent_packet_drop_)
      : bitrate(bitrate_),
        latency(latency_),
        percent_packet_drop(percent_packet_drop_) {}
  bool operator<=(const MeasuringPoint& other) const {
    return bitrate >= other.bitrate && latency <= other.latency &&
           percent_packet_drop <= other.percent_packet_drop;
  }
  bool operator>=(const MeasuringPoint& other) const {
    return bitrate <= other.bitrate && latency >= other.latency &&
           percent_packet_drop >= other.percent_packet_drop;
  }

  std::string AsString() const {
    return base::StringPrintf(
        "%f Mbit/s %f ms %f %% ", bitrate, latency, percent_packet_drop);
  }

  double bitrate;
  double latency;
  double percent_packet_drop;
};

class RunOneBenchmark {
 public:
  RunOneBenchmark()
      : start_time_(),
        task_runner_(new FakeSingleThreadTaskRunner(&testing_clock_)),
        testing_clock_sender_(&testing_clock_),
        task_runner_sender_(
            new test::SkewedSingleThreadTaskRunner(task_runner_)),
        testing_clock_receiver_(&testing_clock_),
        task_runner_receiver_(
            new test::SkewedSingleThreadTaskRunner(task_runner_)),
        cast_environment_sender_(new CastEnvironment(&testing_clock_sender_,
                                                     task_runner_sender_,
                                                     task_runner_sender_,
                                                     task_runner_sender_)),
        cast_environment_receiver_(new CastEnvironment(&testing_clock_receiver_,
                                                       task_runner_receiver_,
                                                       task_runner_receiver_,
                                                       task_runner_receiver_)),
        video_bytes_encoded_(0),
        audio_bytes_encoded_(0),
        frames_sent_(0) {
    testing_clock_.Advance(
        base::TimeDelta::FromMilliseconds(kStartMillisecond));
  }

  void Configure(Codec video_codec,
                 Codec audio_codec) {
    audio_sender_config_ = GetDefaultAudioSenderConfig();
    audio_sender_config_.min_playout_delay =
        audio_sender_config_.max_playout_delay =
            base::TimeDelta::FromMilliseconds(kTargetPlayoutDelayMs);
    audio_sender_config_.codec = audio_codec;

    audio_receiver_config_ = GetDefaultAudioReceiverConfig();
    audio_receiver_config_.rtp_max_delay_ms =
        audio_sender_config_.max_playout_delay.InMicroseconds();
    audio_receiver_config_.codec = audio_codec;

    video_sender_config_ = GetDefaultVideoSenderConfig();
    video_sender_config_.min_playout_delay =
        video_sender_config_.max_playout_delay =
            base::TimeDelta::FromMilliseconds(kTargetPlayoutDelayMs);
    video_sender_config_.max_bitrate = 4000000;
    video_sender_config_.min_bitrate = 4000000;
    video_sender_config_.start_bitrate = 4000000;
    video_sender_config_.codec = video_codec;

    video_receiver_config_ = GetDefaultVideoReceiverConfig();
    video_receiver_config_.rtp_max_delay_ms = kTargetPlayoutDelayMs;
    video_receiver_config_.codec = video_codec;

    DCHECK_GT(video_sender_config_.max_frame_rate, 0);
    frame_duration_ = base::TimeDelta::FromSecondsD(
        1.0 / video_sender_config_.max_frame_rate);
  }

  void SetSenderClockSkew(double skew, base::TimeDelta offset) {
    testing_clock_sender_.SetSkew(skew, offset);
    task_runner_sender_->SetSkew(1.0 / skew);
  }

  void SetReceiverClockSkew(double skew, base::TimeDelta offset) {
    testing_clock_receiver_.SetSkew(skew, offset);
    task_runner_receiver_->SetSkew(1.0 / skew);
  }

  void Create(const MeasuringPoint& p);

  void ReceivePacket(std::unique_ptr<Packet> packet) {
    cast_receiver_->ReceivePacket(std::move(packet));
  }

  virtual ~RunOneBenchmark() {
    cast_sender_.reset();
    cast_receiver_.reset();
    task_runner_->RunTasks();
  }

  base::TimeDelta VideoTimestamp(int frame_number) {
    return frame_number * base::TimeDelta::FromSecondsD(
                              1.0 / video_sender_config_.max_frame_rate);
  }

  void SendFakeVideoFrame() {
    // NB: Blackframe with timestamp
    cast_sender_->video_frame_input()->InsertRawVideoFrame(
        media::VideoFrame::CreateColorFrame(gfx::Size(2, 2), 0x00, 0x80, 0x80,
                                            VideoTimestamp(frames_sent_)),
        testing_clock_sender_.NowTicks());
    frames_sent_++;
  }

  void RunTasks(base::TimeDelta duration) {
    task_runner_->Sleep(duration);
  }

  void BasicPlayerGotVideoFrame(scoped_refptr<media::VideoFrame> video_frame,
                                base::TimeTicks render_time,
                                bool continuous) {
    video_ticks_.push_back(
        std::make_pair(testing_clock_receiver_.NowTicks(), render_time));
    cast_receiver_->RequestDecodedVideoFrame(base::Bind(
        &RunOneBenchmark::BasicPlayerGotVideoFrame, base::Unretained(this)));
  }

  void BasicPlayerGotAudioFrame(std::unique_ptr<AudioBus> audio_bus,
                                base::TimeTicks playout_time,
                                bool is_continuous) {
    audio_ticks_.push_back(
        std::make_pair(testing_clock_receiver_.NowTicks(), playout_time));
    cast_receiver_->RequestDecodedAudioFrame(base::Bind(
        &RunOneBenchmark::BasicPlayerGotAudioFrame, base::Unretained(this)));
  }

  void StartBasicPlayer() {
    cast_receiver_->RequestDecodedVideoFrame(base::Bind(
        &RunOneBenchmark::BasicPlayerGotVideoFrame, base::Unretained(this)));
    cast_receiver_->RequestDecodedAudioFrame(base::Bind(
        &RunOneBenchmark::BasicPlayerGotAudioFrame, base::Unretained(this)));
  }

  std::unique_ptr<test::PacketPipe> CreateSimplePipe(const MeasuringPoint& p) {
    std::unique_ptr<test::PacketPipe> pipe = test::NewBuffer(65536, p.bitrate);
    pipe->AppendToPipe(test::NewRandomDrop(p.percent_packet_drop / 100.0));
    pipe->AppendToPipe(test::NewConstantDelay(p.latency / 1000.0));
    return pipe;
  }

  void Run(const MeasuringPoint& p) {
    available_bitrate_ = p.bitrate;
    Configure(CODEC_VIDEO_FAKE, CODEC_AUDIO_PCM16);
    Create(p);
    StartBasicPlayer();

    for (int frame = 0; frame < 1000; frame++) {
      SendFakeVideoFrame();
      RunTasks(frame_duration_);
    }
    RunTasks(100 * frame_duration_);  // Empty the pipeline.
    VLOG(1) << "=============INPUTS============";
    VLOG(1) << "Bitrate: " << p.bitrate << " mbit/s";
    VLOG(1) << "Latency: " << p.latency << " ms";
    VLOG(1) << "Packet drop drop: " << p.percent_packet_drop << "%";
    VLOG(1) << "=============OUTPUTS============";
    VLOG(1) << "Frames lost: " << frames_lost();
    VLOG(1) << "Late frames: " << late_frames();
    VLOG(1) << "Playout margin: " << frame_playout_buffer().AsString();
    VLOG(1) << "Video bandwidth used: " << video_bandwidth() << " mbit/s ("
            << (video_bandwidth() * 100 / desired_video_bitrate()) << "%)";
    VLOG(1) << "Good run: " << SimpleGood();
  }

  // Metrics
  int frames_lost() const { return frames_sent_ - video_ticks_.size(); }

  int late_frames() const {
    int frames = 0;
    // Ignore the first two seconds of video or so.
    for (size_t i = 60; i < video_ticks_.size(); i++) {
      if (video_ticks_[i].first > video_ticks_[i].second) {
        frames++;
      }
    }
    return frames;
  }

  test::MeanAndError frame_playout_buffer() const {
    std::vector<double> values;
    for (size_t i = 0; i < video_ticks_.size(); i++) {
      values.push_back(
          (video_ticks_[i].second - video_ticks_[i].first).InMillisecondsF());
    }
    return test::MeanAndError(values);
  }

  // Mbits per second
  double video_bandwidth() const {
    double seconds = (frame_duration_.InSecondsF() * frames_sent_);
    double megabits = video_bytes_encoded_ * 8 / 1000000.0;
    return megabits / seconds;
  }

  // Mbits per second
  double audio_bandwidth() const {
    double seconds = (frame_duration_.InSecondsF() * frames_sent_);
    double megabits = audio_bytes_encoded_ * 8 / 1000000.0;
    return megabits / seconds;
  }

  double desired_video_bitrate() {
    return std::min<double>(available_bitrate_,
                            video_sender_config_.max_bitrate / 1000000.0);
  }

  bool SimpleGood() {
    return frames_lost() <= 1 && late_frames() <= 1 &&
           video_bandwidth() > desired_video_bitrate() * 0.8 &&
           video_bandwidth() < desired_video_bitrate() * 1.2;
  }

 private:
  FrameReceiverConfig audio_receiver_config_;
  FrameReceiverConfig video_receiver_config_;
  FrameSenderConfig audio_sender_config_;
  FrameSenderConfig video_sender_config_;

  base::TimeTicks start_time_;

  // These run in "test time"
  base::SimpleTestTickClock testing_clock_;
  scoped_refptr<FakeSingleThreadTaskRunner> task_runner_;

  // These run on the sender timeline.
  test::SkewedTickClock testing_clock_sender_;
  scoped_refptr<test::SkewedSingleThreadTaskRunner> task_runner_sender_;

  // These run on the receiver timeline.
  test::SkewedTickClock testing_clock_receiver_;
  scoped_refptr<test::SkewedSingleThreadTaskRunner> task_runner_receiver_;

  scoped_refptr<CastEnvironment> cast_environment_sender_;
  scoped_refptr<CastEnvironment> cast_environment_receiver_;

  LoopBackTransport* receiver_to_sender_;  // Owned by CastTransportImpl.
  LoopBackTransport* sender_to_receiver_;  // Owned by CastTransportImpl.
  CastTransportWrapper transport_sender_;
  std::unique_ptr<CastTransport> transport_receiver_;
  uint64_t video_bytes_encoded_;
  uint64_t audio_bytes_encoded_;

  std::unique_ptr<CastReceiver> cast_receiver_;
  std::unique_ptr<CastSender> cast_sender_;

  int frames_sent_;
  base::TimeDelta frame_duration_;
  double available_bitrate_;
  std::vector<std::pair<base::TimeTicks, base::TimeTicks> > audio_ticks_;
  std::vector<std::pair<base::TimeTicks, base::TimeTicks> > video_ticks_;
};

namespace {

class TransportClient : public CastTransport::Client {
 public:
  explicit TransportClient(RunOneBenchmark* run_one_benchmark)
      : run_one_benchmark_(run_one_benchmark) {}

  void OnStatusChanged(CastTransportStatus status) final {
    EXPECT_EQ(TRANSPORT_STREAM_INITIALIZED, status);
  }
  void OnLoggingEventsReceived(
      std::unique_ptr<std::vector<FrameEvent>> frame_events,
      std::unique_ptr<std::vector<PacketEvent>> packet_events) final {}
  void ProcessRtpPacket(std::unique_ptr<Packet> packet) final {
    if (run_one_benchmark_)
      run_one_benchmark_->ReceivePacket(std::move(packet));
  }

 private:
  RunOneBenchmark* const run_one_benchmark_;

  DISALLOW_COPY_AND_ASSIGN(TransportClient);
};

}  // namepspace

void RunOneBenchmark::Create(const MeasuringPoint& p) {
  sender_to_receiver_ = new LoopBackTransport(cast_environment_sender_);
  transport_sender_.Init(
      new CastTransportImpl(
          &testing_clock_sender_, base::TimeDelta::FromSeconds(1),
          std::make_unique<TransportClient>(nullptr),
          base::WrapUnique(sender_to_receiver_), task_runner_sender_),
      &video_bytes_encoded_, &audio_bytes_encoded_);

  receiver_to_sender_ = new LoopBackTransport(cast_environment_receiver_);
  transport_receiver_.reset(new CastTransportImpl(
      &testing_clock_receiver_, base::TimeDelta::FromSeconds(1),
      std::make_unique<TransportClient>(this),
      base::WrapUnique(receiver_to_sender_), task_runner_receiver_));

  cast_receiver_ =
      CastReceiver::Create(cast_environment_receiver_, audio_receiver_config_,
                           video_receiver_config_, transport_receiver_.get());

  cast_sender_ =
      CastSender::Create(cast_environment_sender_, &transport_sender_);

  cast_sender_->InitializeAudio(audio_sender_config_,
                                base::Bind(&ExpectAudioSuccess));
  cast_sender_->InitializeVideo(video_sender_config_,
                                base::Bind(&ExpectVideoSuccess),
                                CreateDefaultVideoEncodeAcceleratorCallback(),
                                CreateDefaultVideoEncodeMemoryCallback());

  receiver_to_sender_->Initialize(CreateSimplePipe(p),
                                  transport_sender_.PacketReceiverForTesting(),
                                  task_runner_, &testing_clock_);
  sender_to_receiver_->Initialize(
      CreateSimplePipe(p), transport_receiver_->PacketReceiverForTesting(),
      task_runner_, &testing_clock_);

  task_runner_->RunTasks();
}

enum CacheResult { FOUND_TRUE, FOUND_FALSE, NOT_FOUND };

template <class T>
class BenchmarkCache {
 public:
  CacheResult Lookup(const T& x) {
    base::AutoLock key(lock_);
    for (size_t i = 0; i < results_.size(); i++) {
      if (results_[i].second) {
        if (x <= results_[i].first) {
          VLOG(2) << "TRUE because: " << x.AsString()
                  << " <= " << results_[i].first.AsString();
          return FOUND_TRUE;
        }
      } else {
        if (x >= results_[i].first) {
          VLOG(2) << "FALSE because: " << x.AsString()
                  << " >= " << results_[i].first.AsString();
          return FOUND_FALSE;
        }
      }
    }
    return NOT_FOUND;
  }

  void Add(const T& x, bool result) {
    base::AutoLock key(lock_);
    VLOG(2) << "Cache Insert: " << x.AsString() << " = " << result;
    results_.push_back(std::make_pair(x, result));
  }

 private:
  base::Lock lock_;
  std::vector<std::pair<T, bool> > results_;
};

struct SearchVariable {
  SearchVariable() : base(0.0), grade(0.0) {}
  SearchVariable(double b, double g) : base(b), grade(g) {}
  SearchVariable blend(const SearchVariable& other, double factor) {
    CHECK_GE(factor, 0);
    CHECK_LE(factor, 1.0);
    return SearchVariable(base * (1 - factor) + other.base * factor,
                          grade * (1 - factor) + other.grade * factor);
  }
  double value(double x) const { return base + grade * x; }
  double base;
  double grade;
};

struct SearchVector {
  SearchVector blend(const SearchVector& other, double factor) {
    SearchVector ret;
    ret.bitrate = bitrate.blend(other.bitrate, factor);
    ret.latency = latency.blend(other.latency, factor);
    ret.packet_drop = packet_drop.blend(other.packet_drop, factor);
    return ret;
  }

  SearchVector average(const SearchVector& other) {
    return blend(other, 0.5);
  }

  MeasuringPoint GetMeasuringPoint(double v) const {
    return MeasuringPoint(
        bitrate.value(-v), latency.value(v), packet_drop.value(v));
  }
  std::string AsString(double v) { return GetMeasuringPoint(v).AsString(); }

  SearchVariable bitrate;
  SearchVariable latency;
  SearchVariable packet_drop;
};

class CastBenchmark {
 public:
  bool RunOnePoint(const SearchVector& v, double multiplier) {
    MeasuringPoint p = v.GetMeasuringPoint(multiplier);
    VLOG(1) << "RUN: v = " << multiplier << " p = " << p.AsString();
    if (p.bitrate <= 0) {
      return false;
    }
    switch (cache_.Lookup(p)) {
      case FOUND_TRUE:
        return true;
      case FOUND_FALSE:
        return false;
      case NOT_FOUND:
        // Keep going
        break;
    }
    bool result = true;
    for (int tries = 0; tries < 3 && result; tries++) {
      RunOneBenchmark benchmark;
      benchmark.Run(p);
      result &= benchmark.SimpleGood();
    }
    cache_.Add(p, result);
    return result;
  }

  void BinarySearch(SearchVector v, double accuracy) {
    double min = 0.0;
    double max = 1.0;
    while (RunOnePoint(v, max)) {
      min = max;
      max *= 2;
    }

    while (max - min > accuracy) {
      double avg = (min + max) / 2;
      if (RunOnePoint(v, avg)) {
        min = avg;
      } else {
        max = avg;
      }
    }

    // Print a data point to stdout.
    base::AutoLock key(lock_);
    MeasuringPoint p = v.GetMeasuringPoint(min);
    fprintf(stdout, "%f %f %f\n", p.bitrate, p.latency, p.percent_packet_drop);
    fflush(stdout);
  }

  void SpanningSearch(int max,
                      int x,
                      int y,
                      int skip,
                      SearchVector a,
                      SearchVector b,
                      SearchVector c,
                      double accuracy,
                      std::vector<std::unique_ptr<base::Thread>>* threads) {
    static int thread_num = 0;
    if (x > max) return;
    if (skip > max) {
      if (y > x) return;
      SearchVector ab = a.blend(b, static_cast<double>(x) / max);
      SearchVector ac = a.blend(c, static_cast<double>(x) / max);
      SearchVector v = ab.blend(ac, x == y ? 1.0 : static_cast<double>(y) / x);
      thread_num++;
      (*threads)[thread_num % threads->size()]->task_runner()->PostTask(
          FROM_HERE, base::BindOnce(&CastBenchmark::BinarySearch,
                                    base::Unretained(this), v, accuracy));
    } else {
      skip *= 2;
      SpanningSearch(max, x, y, skip, a, b, c, accuracy, threads);
      SpanningSearch(max, x + skip, y + skip, skip, a, b, c, accuracy, threads);
      SpanningSearch(max, x + skip, y, skip, a, b, c, accuracy, threads);
      SpanningSearch(max, x, y + skip, skip, a, b, c, accuracy, threads);
    }
  }

  void Run() {
    // Spanning search.

    std::vector<std::unique_ptr<base::Thread>> threads;
    for (int i = 0; i < 16; i++) {
      threads.push_back(std::make_unique<base::Thread>(
          base::StringPrintf("cast_bench_thread_%d", i)));
      threads[i]->Start();
    }

    if (base::CommandLine::ForCurrentProcess()->HasSwitch("single-run")) {
      SearchVector a;
      a.bitrate.base = 100.0;
      a.bitrate.grade = 1.0;
      a.latency.grade = 1.0;
      a.packet_drop.grade = 1.0;
      threads[0]->task_runner()->PostTask(
          FROM_HERE,
          base::BindOnce(base::IgnoreResult(&CastBenchmark::RunOnePoint),
                         base::Unretained(this), a, 1.0));
    } else {
      SearchVector a, b, c;
      a.bitrate.base = b.bitrate.base = c.bitrate.base = 100.0;
      a.bitrate.grade = 1.0;
      b.latency.grade = 1.0;
      c.packet_drop.grade = 1.0;

      SpanningSearch(512,
                     0,
                     0,
                     1,
                     a,
                     b,
                     c,
                     0.01,
                     &threads);
    }

    for (size_t i = 0; i < threads.size(); i++) {
      threads[i]->Stop();
    }
  }

 private:
  BenchmarkCache<MeasuringPoint> cache_;
  base::Lock lock_;
};

}  // namespace cast
}  // namespace media

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);
  media::cast::CastBenchmark benchmark;
  if (getenv("PROFILE_FILE")) {
    std::string profile_file(getenv("PROFILE_FILE"));
    base::debug::StartProfiling(profile_file);
    benchmark.Run();
    base::debug::StopProfiling();
  } else {
    benchmark.Run();
  }
}
