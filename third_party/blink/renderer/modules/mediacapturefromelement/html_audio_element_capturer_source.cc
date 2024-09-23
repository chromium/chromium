// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediacapturefromelement/html_audio_element_capturer_source.h"

#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_renderer_sink.h"
#include "third_party/blink/public/platform/web_audio_source_provider_impl.h"
#include "third_party/blink/public/platform/web_media_player.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

// static
HtmlAudioElementCapturerSource*
HtmlAudioElementCapturerSource::CreateFromWebMediaPlayerImpl(
    blink::WebMediaPlayer* player,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(player);
  return new HtmlAudioElementCapturerSource(player->GetAudioSourceProvider(),
                                            std::move(task_runner));
}

HtmlAudioElementCapturerSource::HtmlAudioElementCapturerSource(
    scoped_refptr<blink::WebAudioSourceProviderImpl> audio_source,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : blink::MediaStreamAudioSource(std::move(task_runner),
                                    true /* is_local_source */),
      audio_source_(std::move(audio_source)),
      is_started_(false),
      last_sample_rate_(0),
      last_num_channels_(0),
      last_bus_frames_(0) {
  DCHECK(audio_source_);
}

HtmlAudioElementCapturerSource::~HtmlAudioElementCapturerSource() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  EnsureSourceIsStopped();
}

bool HtmlAudioElementCapturerSource::EnsureSourceIsStarted() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (audio_source_ && !is_started_) {
    GetTaskRunner()->PostTask(
        FROM_HERE,
        WTF::BindOnce(&HtmlAudioElementCapturerSource::SetAudioCallback,
                      weak_factory_.GetWeakPtr()));
    is_started_ = true;
  }
  return is_started_;
}

void HtmlAudioElementCapturerSource::SetAudioCallback() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (audio_source_ && is_started_) {
    // WTF::Unretained() is safe here since EnsureSourceIsStopped() guarantees
    // no more calls to OnAudioBus().
    audio_source_->SetCopyAudioCallback(ConvertToBaseRepeatingCallback(
        CrossThreadBindRepeating(&HtmlAudioElementCapturerSource::OnAudioBus,
                                 CrossThreadUnretained(this))));
  }
}

void HtmlAudioElementCapturerSource::EnsureSourceIsStopped() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!is_started_)
    return;

  if (audio_source_) {
    audio_source_->ClearCopyAudioCallback();
    audio_source_ = nullptr;
  }
  is_started_ = false;
}

void HtmlAudioElementCapturerSource::OnAudioBus(
    std::unique_ptr<media::AudioBus> audio_bus,
    uint32_t frames_delayed,
    int sample_rate) {
  const base::TimeTicks capture_time =
      base::TimeTicks::Now() -
      base::Microseconds(base::Time::kMicrosecondsPerSecond * frames_delayed /
                         sample_rate);

  if (sample_rate != last_sample_rate_ ||
      audio_bus->channels() != last_num_channels_ ||
      audio_bus->frames() != last_bus_frames_) {
    blink::MediaStreamAudioSource::SetFormat(media::AudioParameters(
        media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
        media::ChannelLayoutConfig::Guess(audio_bus->channels()), sample_rate,
        audio_bus->frames()));
    last_sample_rate_ = sample_rate;
    last_num_channels_ = audio_bus->channels();
    last_bus_frames_ = audio_bus->frames();
  }

  blink::MediaStreamAudioSource::DeliverDataToTracks(*audio_bus, capture_time,
                                                     {});
}

}  // namespace blink
