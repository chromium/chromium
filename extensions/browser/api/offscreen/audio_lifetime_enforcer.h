// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_OFFSCREEN_AUDIO_LIFETIME_ENFORCER_H_
#define EXTENSIONS_BROWSER_API_OFFSCREEN_AUDIO_LIFETIME_ENFORCER_H_

#include "base/auto_reset.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/api/offscreen/offscreen_document_lifetime_enforcer.h"

namespace extensions {

// The lifetime enforcer for an offscreen document playing audio. A document
// is considered inactive if a certain amount of time has passed without playing
// audio.
class AudioLifetimeEnforcer : public OffscreenDocumentLifetimeEnforcer,
                              public content::WebContentsObserver {
 public:
  AudioLifetimeEnforcer(OffscreenDocumentHost* offscreen_document,
                        TerminationCallback termination_callback,
                        NotifyInactiveCallback notify_inactive_callback);
  AudioLifetimeEnforcer(const AudioLifetimeEnforcer&) = delete;
  AudioLifetimeEnforcer& operator=(const AudioLifetimeEnforcer&) = delete;
  ~AudioLifetimeEnforcer() override;

  // Overrides the time necessary before a document is considered inactive.
  static base::AutoReset<base::TimeDelta> SetTimeoutForTesting(
      base::TimeDelta timeout);

  // OffscreenDocumentLifetimeEnforcer:
  bool IsActive() override;

 private:
  // content::WebContentsObserver:
  void OnAudioStateChanged(bool audible) override;

  // Posts the delayed task for a document being considered inactive.
  void PostTimeoutTask();

  // Triggered after a set amount of time of the document not playing audio, at
  // which point it should be considered inactive.
  void OnAudioTimeout();

  // Whether the offscreen document was recently active. We start this off as
  // `true` to allow the extension to load audio.
  bool was_recently_active_ = true;

  base::WeakPtrFactory<AudioLifetimeEnforcer> weak_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_OFFSCREEN_AUDIO_LIFETIME_ENFORCER_H_
