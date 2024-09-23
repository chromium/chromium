// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_output_dispatcher_impl.h"

#include <algorithm>
#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/not_fatal_until.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "media/audio/audio_logging.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_output_proxy.h"

namespace media {

AudioOutputDispatcherImpl::AudioOutputDispatcherImpl(
    AudioManager* audio_manager,
    const AudioParameters& params,
    const std::string& output_device_id,
    base::TimeDelta close_delay)
    : AudioOutputDispatcher(audio_manager),
      params_(params),
      device_id_(output_device_id),
      idle_proxies_(0),
      close_timer_(FROM_HERE,
                   close_delay,
                   this,
                   &AudioOutputDispatcherImpl::CloseAllIdleStreams),
      audio_stream_id_(0) {
  DCHECK(audio_manager->GetTaskRunner()->BelongsToCurrentThread());
  audio_manager->AddOutputDeviceChangeListener(this);
}

AudioOutputDispatcherImpl::~AudioOutputDispatcherImpl() {
  DCHECK(audio_manager()->GetTaskRunner()->BelongsToCurrentThread());

  // Stop all active streams.
  for (const auto& [proxy, stream] : proxy_to_physical_map_) {
    StopPhysicalStream(stream);
  }

  // Close all idle streams immediately.  The |close_timer_| will handle
  // invalidating any outstanding tasks upon its destruction.
  CloseAllIdleStreams();

  audio_manager()->RemoveOutputDeviceChangeListener(this);

  // All idle physical streams must have been closed during shutdown.
  CHECK(idle_streams_.empty());
}

AudioOutputProxy* AudioOutputDispatcherImpl::CreateStreamProxy() {
  DCHECK(audio_manager()->GetTaskRunner()->BelongsToCurrentThread());
  return new AudioOutputProxy(weak_factory_.GetWeakPtr());
}

bool AudioOutputDispatcherImpl::OpenStream() {
  DCHECK(audio_manager()->GetTaskRunner()->BelongsToCurrentThread());

  // Ensure that there is at least one open stream.
  if (idle_streams_.empty() && !CreateAndOpenStream())
    return false;

  ++idle_proxies_;
  close_timer_.Reset();
  return true;
}

bool AudioOutputDispatcherImpl::StartStream(
    AudioOutputStream::AudioSourceCallback* callback,
    AudioOutputProxy* stream_proxy) {
  DCHECK(audio_manager()->GetTaskRunner()->BelongsToCurrentThread());
  DCHECK(!proxy_to_physical_map_.contains(stream_proxy));

  if (idle_streams_.empty() && !CreateAndOpenStream())
    return false;

  AudioOutputStream* physical_stream = idle_streams_.back();
  idle_streams_.pop_back();

  DCHECK_GT(idle_proxies_, 0u);
  --idle_proxies_;

  double volume = 0;
  stream_proxy->GetVolume(&volume);
  physical_stream->SetVolume(volume);
  DCHECK(base::Contains(audio_logs_, physical_stream));
  AudioLog* const audio_log = audio_logs_[physical_stream].get();
  audio_log->OnSetVolume(volume);
  physical_stream->Start(callback);
  audio_log->OnStarted();
  proxy_to_physical_map_[stream_proxy] = physical_stream;

  close_timer_.Reset();
  return true;
}

void AudioOutputDispatcherImpl::StopStream(AudioOutputProxy* stream_proxy) {
  DCHECK(audio_manager()->GetTaskRunner()->BelongsToCurrentThread());
  auto it = proxy_to_physical_map_.find(stream_proxy);
  CHECK(it != proxy_to_physical_map_.end(), base::NotFatalUntil::M130);
  StopPhysicalStream(it->second);
  proxy_to_physical_map_.erase(it);
  ++idle_proxies_;
}

void AudioOutputDispatcherImpl::StreamVolumeSet(AudioOutputProxy* stream_proxy,
                                                double volume) {
  DCHECK(audio_manager()->GetTaskRunner()->BelongsToCurrentThread());
  auto it = proxy_to_physical_map_.find(stream_proxy);
  if (it != proxy_to_physical_map_.end()) {
    AudioOutputStream* physical_stream = it->second;
    physical_stream->SetVolume(volume);
    DCHECK(base::Contains(audio_logs_, physical_stream));
    audio_logs_[physical_stream]->OnSetVolume(volume);
  }
}

void AudioOutputDispatcherImpl::CloseStream(AudioOutputProxy* stream_proxy) {
  DCHECK(audio_manager()->GetTaskRunner()->BelongsToCurrentThread());
  DCHECK_GT(idle_proxies_, 0u);
  --idle_proxies_;

  // Leave at least a single stream running until the close timer fires to help
  // cycle time when streams are opened and closed repeatedly.
  CloseIdleStreams(std::max(idle_proxies_, static_cast<size_t>(1)));
  close_timer_.Reset();
}

// There is nothing to flush since the phsyical stream is removed during
// StopStream().
void AudioOutputDispatcherImpl::FlushStream(AudioOutputProxy* stream_proxy) {}

void AudioOutputDispatcherImpl::OnDeviceChange() {
  DCHECK(audio_manager()->GetTaskRunner()->BelongsToCurrentThread());

  // We don't want to end up reusing streams which were opened for the wrong
  // default device. We need to post this task so it runs after device changes
  // have been sent to all listeners and they've had time to close streams.
  audio_manager()->GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&AudioOutputDispatcherImpl::CloseAllIdleStreams,
                                weak_factory_.GetWeakPtr()));
}

bool AudioOutputDispatcherImpl::HasOutputProxies() const {
  DCHECK(audio_manager()->GetTaskRunner()->BelongsToCurrentThread());
  return idle_proxies_ || !proxy_to_physical_map_.empty();
}

bool AudioOutputDispatcherImpl::CreateAndOpenStream() {
  DCHECK(audio_manager()->GetTaskRunner()->BelongsToCurrentThread());
  const int stream_id = audio_stream_id_++;
  std::unique_ptr<AudioLog> audio_log = audio_manager()->CreateAudioLog(
      AudioLogFactory::AudioComponent::kAudioOutputStream, stream_id);
  AudioOutputStream* stream = audio_manager()->MakeAudioOutputStream(
      params_, device_id_,
      base::BindRepeating(&AudioLog::OnLogMessage,
                          base::Unretained(audio_log.get())));
  if (!stream)
    return false;

  if (!stream->Open()) {
    stream->Close();
    return false;
  }

  audio_log->OnCreated(params_, device_id_);
  audio_logs_[stream] = std::move(audio_log);

  idle_streams_.push_back(stream);
  return true;
}

void AudioOutputDispatcherImpl::CloseAllIdleStreams() {
  DCHECK(audio_manager()->GetTaskRunner()->BelongsToCurrentThread());
  CloseIdleStreams(0);
}

void AudioOutputDispatcherImpl::CloseIdleStreams(size_t keep_alive) {
  DCHECK(audio_manager()->GetTaskRunner()->BelongsToCurrentThread());
  if (idle_streams_.size() <= keep_alive)
    return;
  for (size_t i = keep_alive; i < idle_streams_.size(); ++i) {
    AudioOutputStream* stream = idle_streams_[i];
    stream->Close();

    auto it = audio_logs_.find(stream);
    CHECK(it != audio_logs_.end(), base::NotFatalUntil::M130);
    it->second->OnClosed();
    audio_logs_.erase(it);
  }
  idle_streams_.erase(idle_streams_.begin() + keep_alive, idle_streams_.end());
}

void AudioOutputDispatcherImpl::StopPhysicalStream(AudioOutputStream* stream) {
  DCHECK(audio_manager()->GetTaskRunner()->BelongsToCurrentThread());
  stream->Stop();
  DCHECK(base::Contains(audio_logs_, stream));
  audio_logs_[stream]->OnStopped();
  idle_streams_.push_back(stream);
  close_timer_.Reset();
}

}  // namespace media
