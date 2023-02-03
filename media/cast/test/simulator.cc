// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Simulate end to end streaming.
//
// Input:
// --source=
//   WebM used as the source of video and audio frames.
// --output=
//   File path to writing out the raw event log of the simulation session.
// --sim-id=
//   Unique simulation ID.
// --target-delay-ms=
//   Target playout delay to configure (integer number of milliseconds).
//   Optional; default is 400.
// --max-frame-rate=
//   The maximum frame rate allowed at any time during the Cast session.
//   Optional; default is 30.
// --source-frame-rate=
//   Overrides the playback rate; the source video will play faster/slower.
// --run-time=
//   In seconds, how long the Cast session runs for.
//   Optional; default is 180.
// --metrics-output=
//   File path to write PSNR and SSIM metrics between source frames and
//   decoded frames. Assumes all encoded frames are decoded.
// --yuv-output=
//   File path to write YUV decoded frames in YUV4MPEG2 format.
// --no-simulation
//   Do not run network simulation.
//
// Output:
// - Raw event log of the simulation session tagged with the unique test ID,
//   written out to the specified file path.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/at_exit.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/containers/queue.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/checked_math.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "media/base/audio_bus.h"
#include "media/base/fake_single_thread_task_runner.h"
#include "media/base/media.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/cast_sender.h"
#include "media/cast/logging/encoding_event_subscriber.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/logging/proto/raw_events.pb.h"
#include "media/cast/logging/raw_event_subscriber_bundle.h"
#include "media/cast/logging/simple_event_subscriber.h"
#include "media/cast/net/cast_transport.h"
#include "media/cast/net/cast_transport_config.h"
#include "media/cast/net/cast_transport_defines.h"
#include "media/cast/net/cast_transport_impl.h"
#include "media/cast/test/fake_media_source.h"
#include "media/cast/test/loopback_transport.h"
#include "media/cast/test/proto/network_simulation_model.pb.h"
#include "media/cast/test/receiver/cast_receiver.h"
#include "media/cast/test/skewed_tick_clock.h"
#include "media/cast/test/utility/audio_utility.h"
#include "media/cast/test/utility/default_config.h"
#include "media/cast/test/utility/test_util.h"
#include "media/cast/test/utility/udp_proxy.h"
#include "media/cast/test/utility/video_utility.h"

using media::cast::proto::IPPModel;
using media::cast::proto::NetworkSimulationModel;
using media::cast::proto::NetworkSimulationModelType;

namespace media {
namespace cast {
namespace {
const char kLibDir[] = "lib-dir";
const char kModelPath[] = "model";
const char kMetricsOutputPath[] = "metrics-output";
const char kOutputPath[] = "output";
const char kMaxFrameRate[] = "max-frame-rate";
const char kNoSimulation[] = "no-simulation";
const char kRunTime[] = "run-time";
const char kSimulationId[] = "sim-id";
const char kSourcePath[] = "source";
const char kSourceFrameRate[] = "source-frame-rate";
const char kTargetDelay[] = "target-delay-ms";
const char kYuvOutputPath[] = "yuv-output";

int GetIntegerSwitchValue(const char* switch_name, int default_value) {
  const std::string as_str =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(switch_name);
  if (as_str.empty())
    return default_value;
  int as_int;
  CHECK(base::StringToInt(as_str, &as_int));
  CHECK_GT(as_int, 0);
  return as_int;
}

void LogAudioOperationalStatus(OperationalStatus status) {
  LOG(INFO) << "Audio status: " << status;
}

void LogVideoOperationalStatus(OperationalStatus status) {
  LOG(INFO) << "Video status: " << status;
}

struct PacketProxy {
  PacketProxy() : receiver(nullptr) {}
  void ReceivePacket(std::unique_ptr<Packet> packet) {
    if (receiver)
      receiver->ReceivePacket(std::move(packet));
  }
  raw_ptr<CastReceiver> receiver;
};

class TransportClient : public CastTransport::Client {
 public:
  TransportClient(LogEventDispatcher* log_event_dispatcher,
                  PacketProxy* packet_proxy)
      : log_event_dispatcher_(log_event_dispatcher),
        packet_proxy_(packet_proxy) {}

  TransportClient(const TransportClient&) = delete;
  TransportClient& operator=(const TransportClient&) = delete;

  void OnStatusChanged(CastTransportStatus status) final {
    LOG(INFO) << "Cast transport status: " << status;
  }
  void OnLoggingEventsReceived(
      std::unique_ptr<std::vector<FrameEvent>> frame_events,
      std::unique_ptr<std::vector<PacketEvent>> packet_events) final {
    DCHECK(log_event_dispatcher_);
    log_event_dispatcher_->DispatchBatchOfEvents(std::move(frame_events),
                                                 std::move(packet_events));
  }
  void ProcessRtpPacket(std::unique_ptr<Packet> packet) final {
    if (packet_proxy_)
      packet_proxy_->ReceivePacket(std::move(packet));
  }

 private:
  const raw_ptr<LogEventDispatcher>
      log_event_dispatcher_;                 // Not owned by this class.
  const raw_ptr<PacketProxy> packet_proxy_;  // Not owned by this class.
};

// Maintains a queue of encoded video frames.
// This works by tracking FRAME_CAPTURE_END and FRAME_ENCODED events.
// If a video frame is detected to be encoded it transfers a frame
// from FakeMediaSource to its internal queue. Otherwise it drops a
// frame from FakeMediaSource.
class EncodedVideoFrameTracker final : public RawEventSubscriber {
 public:
  EncodedVideoFrameTracker(FakeMediaSource* media_source)
      : media_source_(media_source), last_frame_event_type_(UNKNOWN) {}

  EncodedVideoFrameTracker(const EncodedVideoFrameTracker&) = delete;
  EncodedVideoFrameTracker& operator=(const EncodedVideoFrameTracker&) = delete;

  ~EncodedVideoFrameTracker() override = default;

  // RawEventSubscriber implementations.
  void OnReceiveFrameEvent(const FrameEvent& frame_event) override {
    // This method only cares about video FRAME_CAPTURE_END and
    // FRAME_ENCODED events.
    if (frame_event.media_type != VIDEO_EVENT) {
      return;
    }
    if (frame_event.type != FRAME_CAPTURE_END &&
        frame_event.type != FRAME_ENCODED) {
      return;
    }
    // If there are two consecutive FRAME_CAPTURE_END events that means
    // a frame is dropped.
    if (last_frame_event_type_ == FRAME_CAPTURE_END &&
        frame_event.type == FRAME_CAPTURE_END) {
      media_source_->PopOldestInsertedVideoFrame();
    }
    if (frame_event.type == FRAME_ENCODED) {
      video_frames_.push(media_source_->PopOldestInsertedVideoFrame());
    }
    last_frame_event_type_ = frame_event.type;
  }

  void OnReceivePacketEvent(const PacketEvent& packet_event) override {
    // Don't care.
  }

  scoped_refptr<media::VideoFrame> PopOldestEncodedFrame() {
    CHECK(!video_frames_.empty());
    scoped_refptr<media::VideoFrame> video_frame = video_frames_.front();
    video_frames_.pop();
    return video_frame;
  }

 private:
  raw_ptr<FakeMediaSource> media_source_;
  CastLoggingEvent last_frame_event_type_;
  base::queue<scoped_refptr<media::VideoFrame>> video_frames_;
};

// Safely creates a span for data backing a `frame` on specified `plane`.
base::span<const uint8_t> MakeFrameSpan(const media::VideoFrame* frame,
                                        size_t plane) {
  base::CheckedNumeric<size_t> size = frame->stride(plane);
  size *= frame->rows(plane);
  return base::make_span(frame->data(plane), size.ValueOrDie());
}

// Appends a YUV frame in I420 format to the file located at |path|.
void AppendYuvToFile(const base::FilePath& path,
                     scoped_refptr<media::VideoFrame> frame) {
  // Write YUV420 format to file.
  std::string header;
  base::StringAppendF(&header, "FRAME W%d H%d\n", frame->coded_size().width(),
                      frame->coded_size().height());
  AppendToFile(path, header);
  AppendToFile(path, MakeFrameSpan(frame.get(), media::VideoFrame::kYPlane));
  AppendToFile(path, MakeFrameSpan(frame.get(), media::VideoFrame::kUPlane));
  AppendToFile(path, MakeFrameSpan(frame.get(), media::VideoFrame::kVPlane));
}

// A container to save output of GotVideoFrame() for computation based
// on output frames.
struct GotVideoFrameOutput {
  GotVideoFrameOutput() : counter(0) {}
  int counter;
  std::vector<double> psnr;
  std::vector<double> ssim;
};

void GotVideoFrame(GotVideoFrameOutput* metrics_output,
                   const base::FilePath& yuv_output,
                   EncodedVideoFrameTracker* video_frame_tracker,
                   CastReceiver* cast_receiver,
                   scoped_refptr<media::VideoFrame> video_frame,
                   base::TimeTicks render_time,
                   bool continuous) {
  ++metrics_output->counter;
  cast_receiver->RequestDecodedVideoFrame(
      base::BindRepeating(&GotVideoFrame, metrics_output, yuv_output,
                          video_frame_tracker, cast_receiver));

  // If |video_frame_tracker| is available that means we're computing
  // quality metrices.
  if (video_frame_tracker) {
    scoped_refptr<media::VideoFrame> src_frame =
        video_frame_tracker->PopOldestEncodedFrame();
    metrics_output->psnr.push_back(I420PSNR(*src_frame, *video_frame));
    metrics_output->ssim.push_back(I420SSIM(*src_frame, *video_frame));
  }

  if (!yuv_output.empty()) {
    AppendYuvToFile(yuv_output, std::move(video_frame));
  }
}

void GotAudioFrame(int* counter,
                   CastReceiver* cast_receiver,
                   std::unique_ptr<AudioBus> audio_bus,
                   base::TimeTicks playout_time,
                   bool is_continuous) {
  ++*counter;
  cast_receiver->RequestDecodedAudioFrame(
      base::BindRepeating(&GotAudioFrame, counter, cast_receiver));
}

// Run simulation once.
//
// |log_output_path| is the path to write serialized log.
// |extra_data| is extra tagging information to write to log.
void RunSimulation(const base::FilePath& source_path,
                   const base::FilePath& log_output_path,
                   const base::FilePath& metrics_output_path,
                   const base::FilePath& yuv_output_path,
                   const std::string& extra_data,
                   const NetworkSimulationModel& model) {
  // Fake clock. Make sure start time is non zero.
  base::SimpleTestTickClock testing_clock;
  testing_clock.Advance(base::Seconds(1));

  // Task runner.
  scoped_refptr<FakeSingleThreadTaskRunner> task_runner =
      new FakeSingleThreadTaskRunner(&testing_clock);
  base::SingleThreadTaskRunner::CurrentDefaultHandle
      task_runner_current_default_handle(task_runner);

  // CastEnvironments.
  test::SkewedTickClock sender_clock(&testing_clock);
  scoped_refptr<CastEnvironment> sender_env =
      new CastEnvironment(&sender_clock, task_runner, task_runner, task_runner);
  test::SkewedTickClock receiver_clock(&testing_clock);
  scoped_refptr<CastEnvironment> receiver_env = new CastEnvironment(
      &receiver_clock, task_runner, task_runner, task_runner);

  // Event subscriber. Store at most 1 hour of events.
  EncodingEventSubscriber audio_event_subscriber(AUDIO_EVENT, 100 * 60 * 60);
  EncodingEventSubscriber video_event_subscriber(VIDEO_EVENT, 30 * 60 * 60);
  sender_env->logger()->Subscribe(&audio_event_subscriber);
  sender_env->logger()->Subscribe(&video_event_subscriber);

  // Audio sender config.
  FrameSenderConfig audio_sender_config = GetDefaultAudioSenderConfig();
  audio_sender_config.min_playout_delay =
      audio_sender_config.max_playout_delay =
          base::Milliseconds(GetIntegerSwitchValue(kTargetDelay, 400));

  // Audio receiver config.
  FrameReceiverConfig audio_receiver_config = GetDefaultAudioReceiverConfig();
  audio_receiver_config.rtp_max_delay_ms =
      audio_sender_config.max_playout_delay.InMilliseconds();

  // Video sender config.
  FrameSenderConfig video_sender_config = GetDefaultVideoSenderConfig();
  video_sender_config.max_bitrate = 2500000;
  video_sender_config.min_bitrate = 2000000;
  video_sender_config.start_bitrate = 2000000;
  video_sender_config.min_playout_delay =
      video_sender_config.max_playout_delay =
          audio_sender_config.max_playout_delay;
  video_sender_config.max_frame_rate = GetIntegerSwitchValue(kMaxFrameRate, 30);

  // Video receiver config.
  FrameReceiverConfig video_receiver_config = GetDefaultVideoReceiverConfig();
  video_receiver_config.rtp_max_delay_ms =
      video_sender_config.max_playout_delay.InMilliseconds();

  // Loopback transport. Owned by CastTransport.
  LoopBackTransport* receiver_to_sender = new LoopBackTransport(receiver_env);
  LoopBackTransport* sender_to_receiver = new LoopBackTransport(sender_env);

  PacketProxy packet_proxy;

  // Cast receiver.
  std::unique_ptr<CastTransport> transport_receiver(new CastTransportImpl(
      &testing_clock, base::Seconds(1),
      std::make_unique<TransportClient>(receiver_env->logger(), &packet_proxy),
      base::WrapUnique(receiver_to_sender), task_runner));
  std::unique_ptr<CastReceiver> cast_receiver(
      CastReceiver::Create(receiver_env, audio_receiver_config,
                           video_receiver_config, transport_receiver.get()));

  packet_proxy.receiver = cast_receiver.get();

  // Cast sender and transport sender.
  std::unique_ptr<CastTransport> transport_sender(new CastTransportImpl(
      &testing_clock, base::Seconds(1),
      std::make_unique<TransportClient>(sender_env->logger(), nullptr),
      base::WrapUnique(sender_to_receiver), task_runner));
  std::unique_ptr<CastSender> cast_sender(
      CastSender::Create(sender_env, transport_sender.get()));

  // Initialize network simulation model.
  const bool use_network_simulation =
      model.type() == media::cast::proto::INTERRUPTED_POISSON_PROCESS;
  std::unique_ptr<test::InterruptedPoissonProcess> ipp;
  if (use_network_simulation) {
    LOG(INFO) << "Running Poisson based network simulation.";
    const IPPModel& ipp_model = model.ipp();
    std::vector<double> average_rates(ipp_model.average_rate_size());
    base::ranges::copy(ipp_model.average_rate(), average_rates.begin());
    ipp = std::make_unique<test::InterruptedPoissonProcess>(
        average_rates, ipp_model.coef_burstiness(), ipp_model.coef_variance(),
        0);
    receiver_to_sender->Initialize(ipp->NewBuffer(128 * 1024),
                                   transport_sender->PacketReceiverForTesting(),
                                   task_runner, &testing_clock);
    sender_to_receiver->Initialize(
        ipp->NewBuffer(128 * 1024),
        transport_receiver->PacketReceiverForTesting(), task_runner,
        &testing_clock);
  } else {
    LOG(INFO) << "No network simulation.";
    receiver_to_sender->Initialize(std::unique_ptr<test::PacketPipe>(),
                                   transport_sender->PacketReceiverForTesting(),
                                   task_runner, &testing_clock);
    sender_to_receiver->Initialize(
        std::unique_ptr<test::PacketPipe>(),
        transport_receiver->PacketReceiverForTesting(), task_runner,
        &testing_clock);
  }

  // Initialize a fake media source and a tracker to encoded video frames.
  const bool quality_test = !metrics_output_path.empty();
  FakeMediaSource media_source(task_runner, &testing_clock, audio_sender_config,
                               video_sender_config, quality_test);
  std::unique_ptr<EncodedVideoFrameTracker> video_frame_tracker;
  if (quality_test) {
    video_frame_tracker =
        std::make_unique<EncodedVideoFrameTracker>(&media_source);
    sender_env->logger()->Subscribe(video_frame_tracker.get());
  }

  // Quality metrics computed for each frame decoded.
  GotVideoFrameOutput metrics_output;

  // Start receiver.
  int audio_frame_count = 0;
  cast_receiver->RequestDecodedVideoFrame(
      base::BindRepeating(&GotVideoFrame, &metrics_output, yuv_output_path,
                          video_frame_tracker.get(), cast_receiver.get()));
  cast_receiver->RequestDecodedAudioFrame(base::BindRepeating(
      &GotAudioFrame, &audio_frame_count, cast_receiver.get()));

  // Initializing audio and video senders.
  cast_sender->InitializeAudio(audio_sender_config,
                               base::BindOnce(&LogAudioOperationalStatus));
  cast_sender->InitializeVideo(media_source.get_video_config(),
                               base::BindRepeating(&LogVideoOperationalStatus),
                               base::DoNothing());
  task_runner->RunTasks();

  // Truncate YUV files to prepare for writing.
  if (!yuv_output_path.empty()) {
    base::ScopedFILE file(base::OpenFile(yuv_output_path, "wb"));
    if (!file.get()) {
      LOG(ERROR) << "Cannot save YUV output to file.";
      return;
    }
    LOG(INFO) << "Writing YUV output to file: " << yuv_output_path.value();

    // Write YUV4MPEG2 header.
    const std::string header("YUV4MPEG2 W1280 H720 F30000:1001 Ip A1:1 C420\n");
    AppendToFile(yuv_output_path, header);
  }

  // Start sending.
  if (!source_path.empty()) {
    // 0 means using the FPS from the file.
    media_source.SetSourceFile(source_path,
                               GetIntegerSwitchValue(kSourceFrameRate, 0));
  }
  media_source.Start(cast_sender->audio_frame_input(),
                     cast_sender->video_frame_input());

  // By default runs simulation for 3 minutes or the desired duration
  // by using --run-time= flag.
  base::TimeDelta elapsed_time;
  const base::TimeDelta desired_run_time =
      base::Seconds(GetIntegerSwitchValue(kRunTime, 180));
  while (elapsed_time < desired_run_time) {
    // Each step is 100us.
    base::TimeDelta step = base::Microseconds(100);
    task_runner->Sleep(step);
    elapsed_time += step;
  }

  // Unsubscribe from logging events.
  sender_env->logger()->Unsubscribe(&audio_event_subscriber);
  sender_env->logger()->Unsubscribe(&video_event_subscriber);
  if (quality_test)
    sender_env->logger()->Unsubscribe(video_frame_tracker.get());

  // Get event logs for audio and video.
  media::cast::proto::LogMetadata audio_metadata, video_metadata;
  media::cast::FrameEventList audio_frame_events, video_frame_events;
  media::cast::PacketEventList audio_packet_events, video_packet_events;
  audio_metadata.set_extra_data(extra_data);
  video_metadata.set_extra_data(extra_data);
  audio_event_subscriber.GetEventsAndReset(&audio_metadata, &audio_frame_events,
                                           &audio_packet_events);
  video_event_subscriber.GetEventsAndReset(&video_metadata, &video_frame_events,
                                           &video_packet_events);

  // Print simulation results.

  // Compute and print statistics for video:
  //
  // * Total video frames captured.
  // * Total video frames encoded.
  // * Total video frames dropped.
  // * Total video frames received late.
  // * Average target bitrate.
  // * Average encoded bitrate.
  int total_video_frames = 0;
  int encoded_video_frames = 0;
  int dropped_video_frames = 0;
  int late_video_frames = 0;
  int64_t total_delay_of_late_frames_ms = 0;
  int64_t encoded_size = 0;
  int64_t target_bitrate = 0;
  for (size_t i = 0; i < video_frame_events.size(); ++i) {
    const media::cast::proto::AggregatedFrameEvent& event =
        *video_frame_events[i];
    ++total_video_frames;
    if (event.has_encoded_frame_size()) {
      ++encoded_video_frames;
      encoded_size += event.encoded_frame_size();
      target_bitrate += event.target_bitrate();
    } else {
      ++dropped_video_frames;
    }
    if (event.has_delay_millis() && event.delay_millis() < 0) {
      ++late_video_frames;
      total_delay_of_late_frames_ms += -event.delay_millis();
    }
  }

  // Subtract fraction of dropped frames from |elapsed_time| before estimating
  // the average encoded bitrate.
  const base::TimeDelta elapsed_time_undropped =
      total_video_frames <= 0
          ? base::TimeDelta()
          : (elapsed_time * (total_video_frames - dropped_video_frames) /
             total_video_frames);
  constexpr double kKilobitsPerByte = 8.0 / 1000;
  const double avg_encoded_bitrate =
      elapsed_time_undropped <= base::TimeDelta()
          ? 0
          : encoded_size * kKilobitsPerByte * elapsed_time_undropped.ToHz();
  double avg_target_bitrate =
      encoded_video_frames ? target_bitrate / encoded_video_frames / 1000 : 0;

  LOG(INFO) << "Configured target playout delay (ms): "
            << video_receiver_config.rtp_max_delay_ms;
  LOG(INFO) << "Audio frame count: " << audio_frame_count;
  LOG(INFO) << "Inserted video frames: " << total_video_frames;
  LOG(INFO) << "Decoded video frames: " << metrics_output.counter;
  LOG(INFO) << "Dropped video frames: " << dropped_video_frames;
  LOG(INFO) << "Late video frames: " << late_video_frames
            << " (average lateness: "
            << (late_video_frames > 0
                    ? static_cast<double>(total_delay_of_late_frames_ms) /
                          late_video_frames
                    : 0)
            << " ms)";
  LOG(INFO) << "Average encoded bitrate (kbps): " << avg_encoded_bitrate;
  LOG(INFO) << "Average target bitrate (kbps): " << avg_target_bitrate;
  LOG(INFO) << "Writing log: " << log_output_path.value();

  // Truncate file and then write serialized log.
  {
    base::ScopedFILE file(base::OpenFile(log_output_path, "wb"));
    if (!file.get()) {
      LOG(INFO) << "Cannot write to log.";
      return;
    }
  }

  // Write quality metrics.
  if (quality_test) {
    LOG(INFO) << "Writing quality metrics: " << metrics_output_path.value();
    std::string line;
    for (size_t i = 0;
         i < metrics_output.psnr.size() && i < metrics_output.ssim.size();
         ++i) {
      base::StringAppendF(&line, "%f %f\n", metrics_output.psnr[i],
                          metrics_output.ssim[i]);
    }
    WriteFile(metrics_output_path, line.data(), line.length());
  }
}

NetworkSimulationModel DefaultModel() {
  NetworkSimulationModel model;
  model.set_type(cast::proto::INTERRUPTED_POISSON_PROCESS);
  IPPModel* ipp = model.mutable_ipp();
  ipp->set_coef_burstiness(0.609);
  ipp->set_coef_variance(4.1);

  ipp->add_average_rate(0.609);
  ipp->add_average_rate(0.495);
  ipp->add_average_rate(0.561);
  ipp->add_average_rate(0.458);
  ipp->add_average_rate(0.538);
  ipp->add_average_rate(0.513);
  ipp->add_average_rate(0.585);
  ipp->add_average_rate(0.592);
  ipp->add_average_rate(0.658);
  ipp->add_average_rate(0.556);
  ipp->add_average_rate(0.371);
  ipp->add_average_rate(0.595);
  ipp->add_average_rate(0.490);
  ipp->add_average_rate(0.980);
  ipp->add_average_rate(0.781);
  ipp->add_average_rate(0.463);

  return model;
}

bool IsModelValid(const NetworkSimulationModel& model) {
  if (!model.has_type())
    return false;
  NetworkSimulationModelType type = model.type();
  if (type == media::cast::proto::INTERRUPTED_POISSON_PROCESS) {
    if (!model.has_ipp())
      return false;
    const IPPModel& ipp = model.ipp();
    if (ipp.coef_burstiness() <= 0.0 || ipp.coef_variance() <= 0.0)
      return false;
    if (ipp.average_rate_size() == 0)
      return false;
    for (int i = 0; i < ipp.average_rate_size(); i++) {
      if (ipp.average_rate(i) <= 0.0)
        return false;
    }
  }

  return true;
}

NetworkSimulationModel LoadModel(const base::FilePath& model_path) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kNoSimulation)) {
    NetworkSimulationModel model;
    model.set_type(media::cast::proto::NO_SIMULATION);
    return model;
  }
  if (model_path.empty()) {
    LOG(ERROR) << "Model path not set; Using default model.";
    return DefaultModel();
  }
  std::string model_str;
  if (!base::ReadFileToString(model_path, &model_str)) {
    LOG(ERROR) << "Failed to read model file.";
    return DefaultModel();
  }

  NetworkSimulationModel model;
  if (!model.ParseFromString(model_str)) {
    LOG(ERROR) << "Failed to parse model.";
    return DefaultModel();
  }
  if (!IsModelValid(model)) {
    LOG(ERROR) << "Invalid model.";
    return DefaultModel();
  }

  return model;
}

}  // namespace
}  // namespace cast
}  // namespace media

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);
  InitLogging(logging::LoggingSettings());

  const base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  base::FilePath media_path = cmd->GetSwitchValuePath(media::cast::kLibDir);
  if (media_path.empty()) {
    if (!base::PathService::Get(base::DIR_GEN_TEST_DATA_ROOT, &media_path)) {
      LOG(ERROR) << "Failed to load FFmpeg.";
      return 1;
    }
  }

  media::InitializeMediaLibrary();

  base::FilePath source_path =
      cmd->GetSwitchValuePath(media::cast::kSourcePath);
  base::FilePath log_output_path =
      cmd->GetSwitchValuePath(media::cast::kOutputPath);
  if (log_output_path.empty()) {
    base::GetTempDir(&log_output_path);
    log_output_path = log_output_path.AppendASCII("sim-events.gz");
  }
  base::FilePath metrics_output_path =
      cmd->GetSwitchValuePath(media::cast::kMetricsOutputPath);
  base::FilePath yuv_output_path =
      cmd->GetSwitchValuePath(media::cast::kYuvOutputPath);
  std::string sim_id = cmd->GetSwitchValueASCII(media::cast::kSimulationId);

  NetworkSimulationModel model =
      media::cast::LoadModel(cmd->GetSwitchValuePath(media::cast::kModelPath));

  base::Value::Dict values;
  values.Set("sim", true);
  values.Set("sim-id", sim_id);

  std::string extra_data;
  base::JSONWriter::Write(values, &extra_data);

  // Run.
  media::cast::RunSimulation(source_path, log_output_path, metrics_output_path,
                             yuv_output_path, extra_data, model);
  return 0;
}
