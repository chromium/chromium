// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test application that simulates a cast sender - Data can be either generated
// or read from a file.

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/at_exit.h"
#include "base/base_paths.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_executor.h"
#include "base/threading/thread.h"
#include "base/time/default_tick_clock.h"
#include "base/values.h"
#include "media/base/media.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/cast_sender.h"
#include "media/cast/logging/encoding_event_subscriber.h"
#include "media/cast/logging/log_serializer.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/logging/proto/raw_events.pb.h"
#include "media/cast/logging/receiver_time_offset_estimator_impl.h"
#include "media/cast/logging/stats_event_subscriber.h"
#include "media/cast/net/cast_transport.h"
#include "media/cast/net/cast_transport_defines.h"
#include "media/cast/net/udp_transport_impl.h"
#include "media/cast/test/fake_media_source.h"
#include "media/cast/test/utility/default_config.h"
#include "media/cast/test/utility/input_builder.h"

namespace {

// The max allowed size of serialized log.
const int kMaxSerializedLogBytes = 10 * 1000 * 1000;

// Flags for this program:
//
// --address=xx.xx.xx.xx
//   IP address of receiver.
//
// --port=xxxx
//   Port number of receiver.
//
// --source-file=xxx.webm
//   WebM file as source of video frames.
//
// --fps=xx
//   Override framerate of the video stream.
//
// --vary-frame-sizes
//   Randomly vary the video frame sizes at random points in time.  Has no
//   effect if --source-file is being used.
const char kSwitchAddress[] = "address";
const char kSwitchPort[] = "port";
const char kSwitchSourceFile[] = "source-file";
const char kSwitchFps[] = "fps";
const char kSwitchVaryFrameSizes[] = "vary-frame-sizes";

void UpdateCastTransportStatus(
    media::cast::CastTransportStatus status) {
  VLOG(1) << "Transport status: " << status;
}

void QuitLoopOnInitializationResult(media::cast::OperationalStatus result) {
  CHECK(result == media::cast::STATUS_INITIALIZED)
      << "Cast sender uninitialized";
  base::RunLoop::QuitCurrentWhenIdleDeprecated();
}

net::IPEndPoint CreateUDPAddress(const std::string& ip_str, uint16_t port) {
  net::IPAddress ip_address;
  CHECK(ip_address.AssignFromIPLiteral(ip_str));
  return net::IPEndPoint(ip_address, port);
}

void DumpLoggingData(const media::cast::proto::LogMetadata& log_metadata,
                     const media::cast::FrameEventList& frame_events,
                     const media::cast::PacketEventList& packet_events,
                     base::ScopedFILE log_file) {
  VLOG(0) << "Frame map size: " << frame_events.size();
  VLOG(0) << "Packet map size: " << packet_events.size();

  std::unique_ptr<char[]> event_log(new char[kMaxSerializedLogBytes]);
  int event_log_bytes;
  if (!media::cast::SerializeEvents(log_metadata,
                                    frame_events,
                                    packet_events,
                                    true,
                                    kMaxSerializedLogBytes,
                                    event_log.get(),
                                    &event_log_bytes)) {
    VLOG(0) << "Failed to serialize events.";
    return;
  }

  VLOG(0) << "Events serialized length: " << event_log_bytes;

  int ret = fwrite(event_log.get(), 1, event_log_bytes, log_file.get());
  if (ret != event_log_bytes)
    VLOG(0) << "Failed to write logs to file.";
}

void WriteLogsToFileAndDestroySubscribers(
    const scoped_refptr<media::cast::CastEnvironment>& cast_environment,
    std::unique_ptr<media::cast::EncodingEventSubscriber>
        video_event_subscriber,
    std::unique_ptr<media::cast::EncodingEventSubscriber>
        audio_event_subscriber,
    base::ScopedFILE video_log_file,
    base::ScopedFILE audio_log_file) {
  cast_environment->logger()->Unsubscribe(video_event_subscriber.get());
  cast_environment->logger()->Unsubscribe(audio_event_subscriber.get());

  VLOG(0) << "Dumping logging data for video stream.";
  media::cast::proto::LogMetadata log_metadata;
  media::cast::FrameEventList frame_events;
  media::cast::PacketEventList packet_events;
  video_event_subscriber->GetEventsAndReset(
      &log_metadata, &frame_events, &packet_events);

  DumpLoggingData(log_metadata, frame_events, packet_events,
                  std::move(video_log_file));

  VLOG(0) << "Dumping logging data for audio stream.";
  audio_event_subscriber->GetEventsAndReset(
      &log_metadata, &frame_events, &packet_events);

  DumpLoggingData(log_metadata, frame_events, packet_events,
                  std::move(audio_log_file));
}

void WriteStatsAndDestroySubscribers(
    const scoped_refptr<media::cast::CastEnvironment>& cast_environment,
    std::unique_ptr<media::cast::StatsEventSubscriber> video_stats_subscriber,
    std::unique_ptr<media::cast::StatsEventSubscriber> audio_stats_subscriber,
    std::unique_ptr<media::cast::ReceiverTimeOffsetEstimatorImpl> estimator) {
  cast_environment->logger()->Unsubscribe(video_stats_subscriber.get());
  cast_environment->logger()->Unsubscribe(audio_stats_subscriber.get());
  cast_environment->logger()->Unsubscribe(estimator.get());

  std::unique_ptr<base::DictionaryValue> stats =
      video_stats_subscriber->GetStats();
  std::string json;
  base::JSONWriter::WriteWithOptions(
      *stats, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  VLOG(0) << "Video stats: " << json;

  stats = audio_stats_subscriber->GetStats();
  json.clear();
  base::JSONWriter::WriteWithOptions(
      *stats, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  VLOG(0) << "Audio stats: " << json;
}

class TransportClient : public media::cast::CastTransport::Client {
 public:
  explicit TransportClient(
      media::cast::LogEventDispatcher* log_event_dispatcher)
      : log_event_dispatcher_(log_event_dispatcher) {}

  void OnStatusChanged(media::cast::CastTransportStatus status) final {
    VLOG(1) << "Transport status: " << status;
  }
  void OnLoggingEventsReceived(
      std::unique_ptr<std::vector<media::cast::FrameEvent>> frame_events,
      std::unique_ptr<std::vector<media::cast::PacketEvent>> packet_events)
      final {
    DCHECK(log_event_dispatcher_);
    log_event_dispatcher_->DispatchBatchOfEvents(std::move(frame_events),
                                                 std::move(packet_events));
  }
  void ProcessRtpPacket(std::unique_ptr<media::cast::Packet> packet) final {}

 private:
  media::cast::LogEventDispatcher* const
      log_event_dispatcher_;  // Not owned by this class.

  DISALLOW_COPY_AND_ASSIGN(TransportClient);
};

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);
  InitLogging(logging::LoggingSettings());

  // Prepare media module for FFmpeg decoding.
  media::InitializeMediaLibrary();

  base::Thread test_thread("Cast sender test app thread");
  base::Thread audio_thread("Cast audio encoder thread");
  base::Thread video_thread("Cast video encoder thread");
  test_thread.Start();
  audio_thread.Start();
  video_thread.Start();

  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);

  // Default parameters.
  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  std::string remote_ip_address = cmd->GetSwitchValueASCII(kSwitchAddress);
  if (remote_ip_address.empty())
    remote_ip_address = "127.0.0.1";
  int remote_port = 0;
  if (!base::StringToInt(cmd->GetSwitchValueASCII(kSwitchPort), &remote_port) ||
      remote_port < 0 || remote_port > 65535) {
    remote_port = 2344;
  }
  LOG(INFO) << "Sending to " << remote_ip_address << ":" << remote_port
            << ".";

  media::cast::FrameSenderConfig audio_config =
      media::cast::GetDefaultAudioSenderConfig();
  media::cast::FrameSenderConfig video_config =
      media::cast::GetDefaultVideoSenderConfig();

  // Running transport on the main thread.
  // Setting up transport config.
  net::IPEndPoint remote_endpoint =
      CreateUDPAddress(remote_ip_address, static_cast<uint16_t>(remote_port));

  // Enable raw event and stats logging.
  // Running transport on the main thread.
  scoped_refptr<media::cast::CastEnvironment> cast_environment(
      new media::cast::CastEnvironment(
          base::DefaultTickClock::GetInstance(), io_task_executor.task_runner(),
          audio_thread.task_runner(), video_thread.task_runner()));

  // SendProcess initialization.
  std::unique_ptr<media::cast::FakeMediaSource> fake_media_source(
      new media::cast::FakeMediaSource(test_thread.task_runner(),
                                       cast_environment->Clock(), audio_config,
                                       video_config, false));

  int final_fps = 0;
  if (!base::StringToInt(cmd->GetSwitchValueASCII(kSwitchFps),
                         &final_fps)){
    final_fps = 0;
  }
  base::FilePath source_path = cmd->GetSwitchValuePath(kSwitchSourceFile);
  if (!source_path.empty()) {
    LOG(INFO) << "Source: " << source_path.value();
    fake_media_source->SetSourceFile(source_path, final_fps);
  }
  if (cmd->HasSwitch(kSwitchVaryFrameSizes))
    fake_media_source->SetVariableFrameSizeMode(true);

  // CastTransport initialization.
  std::unique_ptr<media::cast::CastTransport> transport_sender =
      media::cast::CastTransport::Create(
          cast_environment->Clock(), base::TimeDelta::FromSeconds(1),
          std::make_unique<TransportClient>(cast_environment->logger()),
          std::make_unique<media::cast::UdpTransportImpl>(
              io_task_executor.task_runner(), net::IPEndPoint(),
              remote_endpoint, base::Bind(&UpdateCastTransportStatus)),
          io_task_executor.task_runner());

  // Set up event subscribers.
  std::unique_ptr<media::cast::EncodingEventSubscriber> video_event_subscriber;
  std::unique_ptr<media::cast::EncodingEventSubscriber> audio_event_subscriber;
  std::string video_log_file_name("/tmp/video_events.log.gz");
  std::string audio_log_file_name("/tmp/audio_events.log.gz");
  LOG(INFO) << "Logging audio events to: " << audio_log_file_name;
  LOG(INFO) << "Logging video events to: " << video_log_file_name;
  video_event_subscriber.reset(new media::cast::EncodingEventSubscriber(
      media::cast::VIDEO_EVENT, 10000));
  audio_event_subscriber.reset(new media::cast::EncodingEventSubscriber(
      media::cast::AUDIO_EVENT, 10000));
  cast_environment->logger()->Subscribe(video_event_subscriber.get());
  cast_environment->logger()->Subscribe(audio_event_subscriber.get());

  // Subscribers for stats.
  std::unique_ptr<media::cast::ReceiverTimeOffsetEstimatorImpl>
      offset_estimator(new media::cast::ReceiverTimeOffsetEstimatorImpl());
  cast_environment->logger()->Subscribe(offset_estimator.get());
  std::unique_ptr<media::cast::StatsEventSubscriber> video_stats_subscriber(
      new media::cast::StatsEventSubscriber(media::cast::VIDEO_EVENT,
                                            cast_environment->Clock(),
                                            offset_estimator.get()));
  std::unique_ptr<media::cast::StatsEventSubscriber> audio_stats_subscriber(
      new media::cast::StatsEventSubscriber(media::cast::AUDIO_EVENT,
                                            cast_environment->Clock(),
                                            offset_estimator.get()));
  cast_environment->logger()->Subscribe(video_stats_subscriber.get());
  cast_environment->logger()->Subscribe(audio_stats_subscriber.get());

  base::ScopedFILE video_log_file(fopen(video_log_file_name.c_str(), "w"));
  if (!video_log_file) {
    VLOG(1) << "Failed to open video log file for writing.";
    exit(-1);
  }

  base::ScopedFILE audio_log_file(fopen(audio_log_file_name.c_str(), "w"));
  if (!audio_log_file) {
    VLOG(1) << "Failed to open audio log file for writing.";
    exit(-1);
  }

  const int logging_duration_seconds = 10;
  io_task_executor.task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&WriteLogsToFileAndDestroySubscribers, cast_environment,
                     std::move(video_event_subscriber),
                     std::move(audio_event_subscriber),
                     std::move(video_log_file), std::move(audio_log_file)),
      base::TimeDelta::FromSeconds(logging_duration_seconds));

  io_task_executor.task_runner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&WriteStatsAndDestroySubscribers, cast_environment,
                     std::move(video_stats_subscriber),
                     std::move(audio_stats_subscriber),
                     std::move(offset_estimator)),
      base::TimeDelta::FromSeconds(logging_duration_seconds));

  // CastSender initialization.
  std::unique_ptr<media::cast::CastSender> cast_sender =
      media::cast::CastSender::Create(cast_environment, transport_sender.get());
  io_task_executor.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&media::cast::CastSender::InitializeVideo,
                     base::Unretained(cast_sender.get()),
                     fake_media_source->get_video_config(),
                     base::Bind(&QuitLoopOnInitializationResult),
                     media::cast::CreateDefaultVideoEncodeAcceleratorCallback(),
                     media::cast::CreateDefaultVideoEncodeMemoryCallback()));
  base::RunLoop().Run();  // Wait for video initialization.
  io_task_executor.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&media::cast::CastSender::InitializeAudio,
                     base::Unretained(cast_sender.get()), audio_config,
                     base::BindRepeating(&QuitLoopOnInitializationResult)));
  base::RunLoop().Run();  // Wait for audio initialization.

  fake_media_source->Start(cast_sender->audio_frame_input(),
                           cast_sender->video_frame_input());
  base::RunLoop().Run();
  return 0;
}
