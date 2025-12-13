// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/loopback_signal_provider.h"

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/trace_event/trace_event.h"
#include "base/types/zip.h"
#include "media/base/audio_bus.h"
#include "media/base/vector_math.h"

namespace audio {

namespace {

// Start with a conservative, but reasonable capture delay that should work for
// most platforms (i.e., not needing an increase during a loopback session).
constexpr base::TimeDelta kInitialCaptureDelay = base::Milliseconds(20);

}  // namespace

LoopbackSignalProvider::LoopbackSignalProvider(
    const media::AudioParameters& output_params,
    std::unique_ptr<LoopbackGroupObserver> loopback_group_observer)
    : output_params_(output_params),
      loopback_group_observer_(std::move(loopback_group_observer)),
      capture_delay_(kInitialCaptureDelay),
      transfer_bus_(media::AudioBus::Create(output_params)) {}

LoopbackSignalProvider::~LoopbackSignalProvider() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("audio", "LoopbackSignalProvider::~LoopbackSignalProvider");

  loopback_group_observer_->StopObserving();
  while (!snoopers_.empty()) {
    OnSourceRemoved(snoopers_.begin()->first);
  }
}

void LoopbackSignalProvider::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT0("audio", "LoopbackSignalProvider::Start");

  loopback_group_observer_->ForEachSource(base::BindRepeating(
      &LoopbackSignalProvider::OnSourceAdded, base::Unretained(this)));
  loopback_group_observer_->StartObserving(this);
}

base::TimeTicks LoopbackSignalProvider::PullLoopbackData(
    media::AudioBus* destination,
    base::TimeTicks capture_time,
    double volume) {
  TRACE_EVENT2(
      "audio", "LoopbackSignalProvider::PullLoopbackData", "capture time (µs)",
      (capture_time - base::TimeTicks()).InMicroseconds(), "volume", volume);
  base::AutoLock scoped_lock(lock_);

  base::TimeTicks delayed_capture_time = capture_time - capture_delay_;
  for (auto& map_entry : snoopers_) {
    const std::optional<base::TimeTicks> suggestion =
        map_entry.second->SuggestLatestRenderTime(destination->frames());
    if (suggestion.value_or(delayed_capture_time) < delayed_capture_time) {
      const base::TimeDelta increase = delayed_capture_time - (*suggestion);
      TRACE_EVENT_INSTANT2("audio", "PullLoopbackData Capture Delay Change",
                           TRACE_EVENT_SCOPE_THREAD, "old capture delay (µs)",
                           capture_delay_.InMicroseconds(), "change (µs)",
                           increase.InMicroseconds());
      delayed_capture_time = *suggestion;
      capture_delay_ += increase;
    }
  }
  TRACE_COUNTER_ID1("audio", "Loopback Capture Delay (µs)", this,
                    capture_delay_.InMicroseconds());

  // Render the audio from each input, apply this stream's volume setting by
  // scaling the data, then mix it all together to form a single audio
  // signal. If there are no snoopers, just render silence.
  auto it = snoopers_.begin();
  if (it == snoopers_.end()) {
    destination->Zero();
  } else {
    // Render the first input's signal directly into `destination`.
    it->second->Render(delayed_capture_time, destination);
    destination->Scale(volume);

    // Render each successive input's signal into `transfer_bus_`, and then
    // mix it into `destination`.
    ++it;
    if (it != snoopers_.end()) {
      do {
        it->second->Render(delayed_capture_time, transfer_bus_.get());
        for (auto [src_ch, dest_ch] : base::zip(transfer_bus_->AllChannels(),
                                                destination->AllChannels())) {
          media::vector_math::FMAC(src_ch, volume, dest_ch);
        }
        ++it;
      } while (it != snoopers_.end());
    }
  }

  return delayed_capture_time;
}

void LoopbackSignalProvider::OnSourceAdded(LoopbackSource* source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!base::TimeTicks::IsHighResolution()) {
    // As of this writing, only machines manufactured before 2008 won't be able
    // to produce high-resolution timestamps. Since the buffer management logic
    // (to mitigate overruns/underruns) depends on them to function correctly,
    // simply return early (i.e., never start snooping on the `member`).
    TRACE_EVENT_INSTANT0("audio",
                         "LoopbackSignalProvider::AddLoopbackSource Rejected",
                         TRACE_EVENT_SCOPE_THREAD);
    return;
  }

  TRACE_EVENT1("audio", "LoopbackSignalProvider::AddLoopbackSource", "source",
               static_cast<void*>(source));

  // Construct the SnooperNode before taking the lock.
  std::unique_ptr<SnooperNode> snooper_node = std::make_unique<SnooperNode>(
      source->GetAudioParameters(), output_params_);

  base::AutoLock scoped_lock(lock_);
  // Dynamically creates a unique pointer, potentially blocking the realtime
  // Audio thread. Consider optimizing this.
  const auto emplace_result =
      snoopers_.emplace(source, std::move(snooper_node));
  DCHECK(emplace_result.second);  // There was no pre-existing map entry.
  source->StartSnooping(emplace_result.first->second.get());
}

void LoopbackSignalProvider::OnSourceRemoved(LoopbackSource* source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT1("audio", "LoopbackSignalProvider::RemoveLoopbackSource",
               "source", static_cast<void*>(source));

  // Delete the snooper after releasing the lock.
  std::unique_ptr<SnooperNode> snooper_to_delete;
  {
    base::AutoLock scoped_lock(lock_);
    const auto snoop_it = snoopers_.find(source);
    if (snoop_it == snoopers_.end()) {
      // See comments about "high-resolution timestamps" in
      // AddLoopbackSource().
      return;
    }
    source->StopSnooping(snoop_it->second.get());
    snooper_to_delete = std::move(snoop_it->second);
    snoopers_.erase(snoop_it);
  }
}

}  // namespace audio
