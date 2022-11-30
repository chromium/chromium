// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_MAC_COREAUDIO_DISPATCH_OVERRIDE_H_
#define MEDIA_AUDIO_MAC_COREAUDIO_DISPATCH_OVERRIDE_H_

namespace media {
// Initializes a CoreAudio hotfix, if supported (macOS >= 10.10).
// See: http://crbug.com/772410
// The hotfix overrides calls to dispatch_get_global_queue() from two CoreAudio
// functions: HALC_IOContext_PauseIO and HALC_IOContext_ResumeIO. These dispatch
// blocks that should execute in-order, but the global queue does not guarantee
// this. When the calls execute out-of-order, we stop receiving callbacks for
// audio streams on one or more devices.
//
// To circumvent this problem, these two functions get handed an internal serial
// queue instead. For all other callers, the override will just defer to the
// normal dispatch_get_global_queue() implementation.
//
// Calls to this function must be serialized. Will do nothing if called when
// already initialized.
//
// Returns true if the hotfix is supported and initialization succeeded, or if
// it was already initialized; false otherwise.
bool InitializeCoreAudioDispatchOverride();
}  // namespace media

#endif  // MEDIA_AUDIO_MAC_COREAUDIO_DISPATCH_OVERRIDE_H_
