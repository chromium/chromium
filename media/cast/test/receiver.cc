// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <climits>
#include <cstdarg>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/circular_deque.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_executor.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager.h"
#include "media/audio/fake_audio_log_factory.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/cast_receiver.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/test/utility/audio_utility.h"
#include "media/cast/test/utility/barcode.h"
#include "media/cast/test/utility/default_config.h"
#include "media/cast/test/utility/in_process_receiver.h"
#include "media/cast/test/utility/input_builder.h"
#include "media/cast/test/utility/standalone_cast_environment.h"
#include "net/base/ip_address.h"

#if defined(USE_X11)
#include "media/cast/test/linux_output_window.h"
#endif  // defined(USE_X11)

namespace media {
namespace cast {

// Settings chosen to match default sender settings.
#define DEFAULT_SEND_PORT "0"
#define DEFAULT_RECEIVE_PORT "2344"
#define DEFAULT_SEND_IP "0.0.0.0"
#define DEFAULT_AUDIO_FEEDBACK_SSRC "2"
#define DEFAULT_AUDIO_INCOMING_SSRC "1"
#define DEFAULT_VIDEO_FEEDBACK_SSRC "12"
#define DEFAULT_VIDEO_INCOMING_SSRC "11"

#if defined(USE_X11)
const char* kVideoWindowWidth = "1280";
const char* kVideoWindowHeight = "720";
#endif  // defined(USE_X11)

void GetPorts(uint16_t* tx_port, uint16_t* rx_port) {
  test::InputBuilder tx_input(
      "Enter send port.", DEFAULT_SEND_PORT, 1, 65535);
  *tx_port = static_cast<uint16_t>(tx_input.GetIntInput());

  test::InputBuilder rx_input(
      "Enter receive port.", DEFAULT_RECEIVE_PORT, 1, 65535);
  *rx_port = static_cast<uint16_t>(rx_input.GetIntInput());
}

std::string GetIpAddress(const std::string& display_text) {
  test::InputBuilder input(display_text, DEFAULT_SEND_IP, INT_MIN, INT_MAX);
  std::string ip_address = input.GetStringInput();
  // Ensure IP address is either the default value or in correct form.
  while (ip_address != DEFAULT_SEND_IP &&
         std::count(ip_address.begin(), ip_address.end(), '.') != 3) {
    ip_address = input.GetStringInput();
  }
  return ip_address;
}

void GetAudioSsrcs(FrameReceiverConfig* audio_config) {
  test::InputBuilder input_tx(
      "Choose audio sender SSRC.", DEFAULT_AUDIO_FEEDBACK_SSRC, 1, INT_MAX);
  audio_config->receiver_ssrc = input_tx.GetIntInput();

  test::InputBuilder input_rx(
      "Choose audio receiver SSRC.", DEFAULT_AUDIO_INCOMING_SSRC, 1, INT_MAX);
  audio_config->sender_ssrc = input_rx.GetIntInput();
}

void GetVideoSsrcs(FrameReceiverConfig* video_config) {
  test::InputBuilder input_tx(
      "Choose video sender SSRC.", DEFAULT_VIDEO_FEEDBACK_SSRC, 1, INT_MAX);
  video_config->receiver_ssrc = input_tx.GetIntInput();

  test::InputBuilder input_rx(
      "Choose video receiver SSRC.", DEFAULT_VIDEO_INCOMING_SSRC, 1, INT_MAX);
  video_config->sender_ssrc = input_rx.GetIntInput();
}

#if defined(USE_X11)
void GetWindowSize(int* width, int* height) {
  // Resolution values based on sender settings
  test::InputBuilder input_w(
      "Choose window width.", kVideoWindowWidth, 144, 1920);
  *width = input_w.GetIntInput();

  test::InputBuilder input_h(
      "Choose window height.", kVideoWindowHeight, 176, 1080);
  *height = input_h.GetIntInput();
}
#endif  // defined(USE_X11)

void GetAudioPayloadtype(FrameReceiverConfig* audio_config) {
  test::InputBuilder input(
      "Choose audio receiver payload type.",
      std::to_string(static_cast<int>(RtpPayloadType::AUDIO_OPUS)),
      static_cast<int>(RtpPayloadType::AUDIO_OPUS) /* low_range */,
      static_cast<int>(RtpPayloadType::AUDIO_LAST) /* high_range */);
  audio_config->rtp_payload_type =
      static_cast<RtpPayloadType>(input.GetIntInput());
}

FrameReceiverConfig GetAudioReceiverConfig() {
  FrameReceiverConfig audio_config = GetDefaultAudioReceiverConfig();
  GetAudioSsrcs(&audio_config);
  GetAudioPayloadtype(&audio_config);
  audio_config.rtp_max_delay_ms = 300;
  return audio_config;
}

void GetVideoPayloadtype(FrameReceiverConfig* video_config) {
  test::InputBuilder input(
      "Choose video receiver payload type.",
      std::to_string(static_cast<int>(RtpPayloadType::VIDEO_VP8)),
      static_cast<int>(RtpPayloadType::VIDEO_VP8) /* low_range */,
      static_cast<int>(RtpPayloadType::LAST) /* high_range */);
  video_config->rtp_payload_type =
      static_cast<RtpPayloadType>(input.GetIntInput());
}

FrameReceiverConfig GetVideoReceiverConfig() {
  FrameReceiverConfig video_config = GetDefaultVideoReceiverConfig();
  GetVideoSsrcs(&video_config);
  GetVideoPayloadtype(&video_config);
  video_config.rtp_max_delay_ms = 300;
  return video_config;
}

AudioParameters ToAudioParameters(const FrameReceiverConfig& config) {
  const int samples_in_10ms = config.rtp_timebase / 100;
  return AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         GuessChannelLayout(config.channels),
                         config.rtp_timebase, samples_in_10ms);
}

// An InProcessReceiver that renders video frames to a LinuxOutputWindow and
// audio frames via Chromium's audio stack.
//
// InProcessReceiver pushes audio and video frames to this subclass, and these
// frames are pushed into a queue.  Then, for audio, the Chromium audio stack
// will make polling calls on a separate, unknown thread whereby audio frames
// are pulled out of the audio queue as needed.  For video, however, NaivePlayer
// is responsible for scheduling updates to the screen itself.  For both, the
// queues are pruned (i.e., received frames are skipped) when the system is not
// able to play back as fast as frames are entering the queue.
//
// This is NOT a good reference implementation for a Cast receiver player since:
// 1. It only skips frames to handle slower-than-expected playout, or halts
//    playback to handle frame underruns.
// 2. It makes no attempt to synchronize the timing of playout of the video
//    frames with the audio frames.
// 3. It does nothing to smooth or hide discontinuities in playback due to
//    timing issues or missing frames.
class NaivePlayer : public InProcessReceiver,
                    public AudioOutputStream::AudioSourceCallback {
 public:
  NaivePlayer(const scoped_refptr<CastEnvironment>& cast_environment,
              const net::IPEndPoint& local_end_point,
              const net::IPEndPoint& remote_end_point,
              const FrameReceiverConfig& audio_config,
              const FrameReceiverConfig& video_config,
              int window_width,
              int window_height)
      : InProcessReceiver(cast_environment,
                          local_end_point,
                          remote_end_point,
                          audio_config,
                          video_config),
        // Maximum age is the duration of 3 video frames.  3 was chosen
        // arbitrarily, but seems to work well.
        max_frame_age_(base::TimeDelta::FromSeconds(1) * 3 /
                           video_config.target_frame_rate),
#if defined(USE_X11)
        render_(0, 0, window_width, window_height, "Cast_receiver"),
#endif  // defined(USE_X11)
        num_video_frames_processed_(0),
        num_audio_frames_processed_(0),
        currently_playing_audio_frame_start_(-1) {}

  ~NaivePlayer() final {}

  void Start() final {
    AudioManager::Get()->GetTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&NaivePlayer::StartAudioOutputOnAudioManagerThread,
                       base::Unretained(this)));
    // Note: No need to wait for audio polling to start since the push-and-pull
    // mechanism is synchronized via the |audio_playout_queue_|.
    InProcessReceiver::Start();
  }

  void Stop() final {
    // First, stop audio output to the Chromium audio stack.
    base::WaitableEvent done(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED);
    DCHECK(!AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
    AudioManager::Get()->GetTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&NaivePlayer::StopAudioOutputOnAudioManagerThread,
                       base::Unretained(this), &done));
    done.Wait();

    // Now, stop receiving new frames.
    InProcessReceiver::Stop();

    // Finally, clear out any frames remaining in the queues.
    while (!audio_playout_queue_.empty()) {
      const std::unique_ptr<AudioBus> to_be_deleted(
          audio_playout_queue_.front().second);
      audio_playout_queue_.pop_front();
    }
    video_playout_queue_.clear();
  }

 private:
  void StartAudioOutputOnAudioManagerThread() {
    DCHECK(AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
    DCHECK(!audio_output_stream_);
    audio_output_stream_.reset(AudioManager::Get()->MakeAudioOutputStreamProxy(
        ToAudioParameters(audio_config()), ""));
    if (audio_output_stream_.get() && audio_output_stream_->Open()) {
      audio_output_stream_->Start(this);
    } else {
      LOG(ERROR) << "Failed to open an audio output stream.  "
                 << "Audio playback disabled.";
      audio_output_stream_.reset();
    }
  }

  void StopAudioOutputOnAudioManagerThread(base::WaitableEvent* done) {
    DCHECK(AudioManager::Get()->GetTaskRunner()->BelongsToCurrentThread());
    if (audio_output_stream_.get()) {
      audio_output_stream_->Stop();
      audio_output_stream_->Close();
      audio_output_stream_.reset();
    }
    done->Signal();
  }

  ////////////////////////////////////////////////////////////////////
  // InProcessReceiver finals.

  void OnVideoFrame(scoped_refptr<VideoFrame> video_frame,
                    base::TimeTicks playout_time,
                    bool is_continuous) final {
    DCHECK(cast_env()->CurrentlyOn(CastEnvironment::MAIN));
    LOG_IF(WARNING, !is_continuous)
        << "Video: Discontinuity in received frames.";
    video_playout_queue_.push_back(std::make_pair(playout_time, video_frame));
    ScheduleVideoPlayout();
    uint16_t frame_no;
    if (media::cast::test::DecodeBarcode(*video_frame, &frame_no)) {
      video_play_times_.insert(
          std::pair<uint16_t, base::TimeTicks>(frame_no, playout_time));
    } else {
      VLOG(2) << "Barcode decode failed!";
    }
  }

  void OnAudioFrame(std::unique_ptr<AudioBus> audio_frame,
                    base::TimeTicks playout_time,
                    bool is_continuous) final {
    DCHECK(cast_env()->CurrentlyOn(CastEnvironment::MAIN));
    LOG_IF(WARNING, !is_continuous)
        << "Audio: Discontinuity in received frames.";
    base::AutoLock auto_lock(audio_lock_);
    uint16_t frame_no;
    if (media::cast::DecodeTimestamp(audio_frame->channel(0),
                                     audio_frame->frames(),
                                     &frame_no)) {
      // Since there are lots of audio packets with the same frame_no,
      // we really want to make sure that we get the playout_time from
      // the first one. If is_continous is true, then it's possible
      // that we already missed the first one.
      if (is_continuous && frame_no == last_audio_frame_no_ + 1) {
        audio_play_times_.insert(
            std::pair<uint16_t, base::TimeTicks>(frame_no, playout_time));
      }
      last_audio_frame_no_ = frame_no;
    } else {
      VLOG(2) << "Audio decode failed!";
      last_audio_frame_no_ = -2;
    }
    audio_playout_queue_.push_back(
        std::make_pair(playout_time, audio_frame.release()));
  }

  // End of InProcessReceiver finals.
  ////////////////////////////////////////////////////////////////////

  ////////////////////////////////////////////////////////////////////
  // AudioSourceCallback implementation.

  int OnMoreData(base::TimeDelta /* delay */,
                 base::TimeTicks /* delay_timestamp */,
                 int /* prior_frames_skipped */,
                 AudioBus* dest) final {
    // Note: This method is being invoked by a separate thread unknown to us
    // (i.e., outside of CastEnvironment).

    int samples_remaining = dest->frames();

    while (samples_remaining > 0) {
      // Get next audio frame ready for playout.
      if (!currently_playing_audio_frame_.get()) {
        base::AutoLock auto_lock(audio_lock_);

        // Prune the queue, skipping entries that are too old.
        // TODO(miu): Use |total_bytes_delay| to account for audio buffering
        // delays upstream.
        const base::TimeTicks earliest_time_to_play =
            cast_env()->Clock()->NowTicks() - max_frame_age_;
        while (!audio_playout_queue_.empty() &&
               audio_playout_queue_.front().first < earliest_time_to_play) {
          PopOneAudioFrame(true);
        }
        if (audio_playout_queue_.empty())
          break;

        currently_playing_audio_frame_ = PopOneAudioFrame(false);
        currently_playing_audio_frame_start_ = 0;
      }

      // Copy some or all of the samples in |currently_playing_audio_frame_| to
      // |dest|.  Once all samples in |currently_playing_audio_frame_| have been
      // consumed, release it.
      const int num_samples_to_copy =
          std::min(samples_remaining,
                   currently_playing_audio_frame_->frames() -
                       currently_playing_audio_frame_start_);
      currently_playing_audio_frame_->CopyPartialFramesTo(
          currently_playing_audio_frame_start_,
          num_samples_to_copy,
          0,
          dest);
      samples_remaining -= num_samples_to_copy;
      currently_playing_audio_frame_start_ += num_samples_to_copy;
      if (currently_playing_audio_frame_start_ ==
              currently_playing_audio_frame_->frames()) {
        currently_playing_audio_frame_.reset();
      }
    }

    // If |dest| has not been fully filled, then an underrun has occurred; and
    // fill the remainder of |dest| with zeros.
    if (samples_remaining > 0) {
      // Note: Only logging underruns after the first frame has been received.
      LOG_IF(WARNING, currently_playing_audio_frame_start_ != -1)
          << "Audio: Playback underrun of " << samples_remaining << " samples!";
      dest->ZeroFramesPartial(dest->frames() - samples_remaining,
                              samples_remaining);
    }

    return dest->frames();
  }

  void OnError() final {
    LOG(ERROR) << "AudioOutputStream reports an error.  "
               << "Playback is unlikely to continue.";
  }

  // End of AudioSourceCallback implementation.
  ////////////////////////////////////////////////////////////////////

  void ScheduleVideoPlayout() {
    DCHECK(cast_env()->CurrentlyOn(CastEnvironment::MAIN));

    // Prune the queue, skipping entries that are too old.
    const base::TimeTicks now = cast_env()->Clock()->NowTicks();
    const base::TimeTicks earliest_time_to_play = now - max_frame_age_;
    while (!video_playout_queue_.empty() &&
           video_playout_queue_.front().first < earliest_time_to_play) {
      PopOneVideoFrame(true);
    }

    // If the queue is not empty, schedule playout of its first frame.
    if (video_playout_queue_.empty()) {
      video_playout_timer_.Stop();
    } else {
      video_playout_timer_.Start(
          FROM_HERE,
          video_playout_queue_.front().first - now,
          base::Bind(&NaivePlayer::PlayNextVideoFrame,
                     base::Unretained(this)));
    }
  }

  void PlayNextVideoFrame() {
    DCHECK(cast_env()->CurrentlyOn(CastEnvironment::MAIN));
    if (!video_playout_queue_.empty()) {
      const scoped_refptr<VideoFrame> video_frame = PopOneVideoFrame(false);
#if defined(USE_X11)
      render_.RenderFrame(*video_frame);
#endif  // defined(USE_X11)
    }
    ScheduleVideoPlayout();
    CheckAVSync();
  }

  scoped_refptr<VideoFrame> PopOneVideoFrame(bool is_being_skipped) {
    DCHECK(cast_env()->CurrentlyOn(CastEnvironment::MAIN));

    if (is_being_skipped) {
      VLOG(1) << "VideoFrame[" << num_video_frames_processed_
              << " (dt=" << (video_playout_queue_.front().first -
                             last_popped_video_playout_time_).InMicroseconds()
              << " usec)]: Skipped.";
    } else {
      VLOG(1) << "VideoFrame[" << num_video_frames_processed_
              << " (dt=" << (video_playout_queue_.front().first -
                             last_popped_video_playout_time_).InMicroseconds()
              << " usec)]: Playing "
              << (cast_env()->Clock()->NowTicks() -
                      video_playout_queue_.front().first).InMicroseconds()
              << " usec later than intended.";
    }

    last_popped_video_playout_time_ = video_playout_queue_.front().first;
    const scoped_refptr<VideoFrame> ret = video_playout_queue_.front().second;
    video_playout_queue_.pop_front();
    ++num_video_frames_processed_;
    return ret;
  }

  std::unique_ptr<AudioBus> PopOneAudioFrame(bool was_skipped) {
    audio_lock_.AssertAcquired();

    if (was_skipped) {
      VLOG(1) << "AudioFrame[" << num_audio_frames_processed_
              << " (dt=" << (audio_playout_queue_.front().first -
                             last_popped_audio_playout_time_).InMicroseconds()
              << " usec)]: Skipped.";
    } else {
      VLOG(1) << "AudioFrame[" << num_audio_frames_processed_
              << " (dt=" << (audio_playout_queue_.front().first -
                             last_popped_audio_playout_time_).InMicroseconds()
              << " usec)]: Playing "
              << (cast_env()->Clock()->NowTicks() -
                      audio_playout_queue_.front().first).InMicroseconds()
              << " usec later than intended.";
    }

    last_popped_audio_playout_time_ = audio_playout_queue_.front().first;
    std::unique_ptr<AudioBus> ret(audio_playout_queue_.front().second);
    audio_playout_queue_.pop_front();
    ++num_audio_frames_processed_;
    return ret;
  }

  void CheckAVSync() {
    if (video_play_times_.size() > 30 &&
        audio_play_times_.size() > 30) {
      size_t num_events = 0;
      base::TimeDelta delta;
      std::map<uint16_t, base::TimeTicks>::iterator audio_iter, video_iter;
      for (video_iter = video_play_times_.begin();
           video_iter != video_play_times_.end();
           ++video_iter) {
        audio_iter = audio_play_times_.find(video_iter->first);
        if (audio_iter != audio_play_times_.end()) {
          num_events++;
          // Positive values means audio is running behind video.
          delta += audio_iter->second - video_iter->second;
        }
      }

      if (num_events > 30) {
        VLOG(0) << "Audio behind by: "
                << (delta / num_events).InMilliseconds()
                << "ms";
        video_play_times_.clear();
        audio_play_times_.clear();
      }
    } else if (video_play_times_.size() + audio_play_times_.size() > 500) {
      // We are decoding audio or video timestamps, but not both, clear it out.
      video_play_times_.clear();
      audio_play_times_.clear();
    }
  }

  // Frames in the queue older than this (relative to NowTicks()) will be
  // dropped (i.e., playback is falling behind).
  const base::TimeDelta max_frame_age_;

  // Outputs created, started, and destroyed by this NaivePlayer.
#if defined(USE_X11)
  test::LinuxOutputWindow render_;
#endif  // defined(USE_X11)
  std::unique_ptr<AudioOutputStream> audio_output_stream_;

  // Video playout queue.
  typedef std::pair<base::TimeTicks, scoped_refptr<VideoFrame> >
      VideoQueueEntry;
  base::circular_deque<VideoQueueEntry> video_playout_queue_;
  base::TimeTicks last_popped_video_playout_time_;
  int64_t num_video_frames_processed_;

  base::OneShotTimer video_playout_timer_;

  // Audio playout queue, synchronized by |audio_lock_|.
  base::Lock audio_lock_;
  typedef std::pair<base::TimeTicks, AudioBus*> AudioQueueEntry;
  base::circular_deque<AudioQueueEntry> audio_playout_queue_;
  base::TimeTicks last_popped_audio_playout_time_;
  int64_t num_audio_frames_processed_;

  // These must only be used on the audio thread calling OnMoreData().
  std::unique_ptr<AudioBus> currently_playing_audio_frame_;
  int currently_playing_audio_frame_start_;

  std::map<uint16_t, base::TimeTicks> audio_play_times_;
  std::map<uint16_t, base::TimeTicks> video_play_times_;
  int32_t last_audio_frame_no_;
};

}  // namespace cast
}  // namespace media

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);
  InitLogging(logging::LoggingSettings());
  base::SingleThreadTaskExecutor main_task_executor;

  scoped_refptr<media::cast::CastEnvironment> cast_environment(
      new media::cast::StandaloneCastEnvironment);

  // Start up Chromium audio system.
  auto audio_manager = media::AudioManager::CreateForTesting(
      std::make_unique<media::TestAudioThread>());
  CHECK(media::AudioManager::Get());

  media::cast::FrameReceiverConfig audio_config =
      media::cast::GetAudioReceiverConfig();
  media::cast::FrameReceiverConfig video_config =
      media::cast::GetVideoReceiverConfig();

  // Determine local and remote endpoints.
  uint16_t remote_port, local_port;
  media::cast::GetPorts(&remote_port, &local_port);
  if (!local_port) {
    LOG(ERROR) << "Invalid local port.";
    return 1;
  }
  std::string remote_ip_address = media::cast::GetIpAddress("Enter remote IP.");
  std::string local_ip_address = media::cast::GetIpAddress("Enter local IP.");
  net::IPAddress remote_ip;
  net::IPAddress local_ip;
  if (!remote_ip.AssignFromIPLiteral(remote_ip_address)) {
    LOG(ERROR) << "Invalid remote IP address.";
    return 1;
  }
  if (!local_ip.AssignFromIPLiteral(local_ip_address)) {
    LOG(ERROR) << "Invalid local IP address.";
    return 1;
  }
  net::IPEndPoint remote_end_point(remote_ip, remote_port);
  net::IPEndPoint local_end_point(local_ip, local_port);

  // Create and start the player.
  int window_width = 0;
  int window_height = 0;
#if defined(USE_X11)
  media::cast::GetWindowSize(&window_width, &window_height);
#endif  // defined(USE_X11)
  media::cast::NaivePlayer player(cast_environment,
                                  local_end_point,
                                  remote_end_point,
                                  audio_config,
                                  video_config,
                                  window_width,
                                  window_height);
  player.Start();

  base::RunLoop().Run();  // Run forever (i.e., until SIGTERM).
  NOTREACHED();
  return 0;
}
