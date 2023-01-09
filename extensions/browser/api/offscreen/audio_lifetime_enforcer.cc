// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/offscreen/audio_lifetime_enforcer.h"

#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "extensions/browser/offscreen_document_host.h"

namespace extensions {

namespace {

// The amount of time for a document to be considered inactive when not playing
// audio.
base::TimeDelta g_audio_timeout = base::Seconds(30);

}  // namespace

AudioLifetimeEnforcer::AudioLifetimeEnforcer(
    OffscreenDocumentHost* offscreen_document,
    TerminationCallback termination_callback,
    NotifyInactiveCallback notify_inactive_callback)
    : OffscreenDocumentLifetimeEnforcer(offscreen_document,
                                        std::move(termination_callback),
                                        std::move(notify_inactive_callback)),
      content::WebContentsObserver(offscreen_document->host_contents()) {
  // Immediately post a timeout; this allows the extension some time to load the
  // audio resource as it first loads, but marks it as inactive if it doesn't
  // play audio after a certain amount of time.
  PostTimeoutTask();
}

AudioLifetimeEnforcer::~AudioLifetimeEnforcer() = default;

// static
base::AutoReset<base::TimeDelta> AudioLifetimeEnforcer::SetTimeoutForTesting(
    base::TimeDelta timeout) {
  return base::AutoReset<base::TimeDelta>(&g_audio_timeout, timeout);
}

bool AudioLifetimeEnforcer::IsActive() {
  return was_recently_active_;
}

void AudioLifetimeEnforcer::OnAudioStateChanged(bool audible) {
  if (audible) {
    // Invalidate any pending timeouts.
    weak_factory_.InvalidateWeakPtrs();
    was_recently_active_ = true;
    return;
  }

  PostTimeoutTask();
}

void AudioLifetimeEnforcer::PostTimeoutTask() {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AudioLifetimeEnforcer::OnAudioTimeout,
                     weak_factory_.GetWeakPtr()),
      g_audio_timeout);
}

void AudioLifetimeEnforcer::OnAudioTimeout() {
  was_recently_active_ = false;
  NotifyInactive();
}

}  // namespace extensions
