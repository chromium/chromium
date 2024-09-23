// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_MAC_PERMISSION_UTILS_H_
#define REMOTING_HOST_MAC_PERMISSION_UTILS_H_

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace remoting::mac {

// Return true if the current process has been granted permission to inject
// input. This will add an entry to the System Preference's Accessibility
// pane (if it doesn't exist already) and it may pop up a system dialog
// informing the user that this app is requesting permission.
bool CanInjectInput();

// Return true if the current process has been granted permission to record
// the screen. This will add an entry to the System Preference's Screen
// Recording pane (if it doesn't exist already) and it may pop up a system
// dialog informing the user that this app is requesting permission.
bool CanRecordScreen();

// Prompts the user to add the current application to the set of trusted
// Accessibility and Screen Recording applications.  The Accessibility
// permission is required for input injection and Screen Recording is required
// for screen capture.  |task_runner| is used to run the dialog message loop.
// TODO(crbug.com/40275162): Remove.
void PromptUserToChangeTrustStateIfNeeded(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner);

// Returns true if the current process has been granted permission to capture
// audio. Unlike the other functions above, this function has no side effect and
// will not request audio capture permission. To do so, call
// RequestAudioCapturePermission() instead.
bool CanCaptureAudio();

// Request audio capture permission. This will add an entry to the System
// Preference's Microphone pane (if it doesn't exist already) and it may pop up
// a system dialog informing the user that this app is requesting permission.
// You may need to explicitly check and request audio capture permission, as
// calling AudioQueue APIs doesn't always trigger the permission request.
// |callback| will be run on the caller's sequence with the grant result. True
// if granted, false otherwise.
// ONLY call this function when CanCaptureAudio() returns false.
void RequestAudioCapturePermission(base::OnceCallback<void(bool)> callback);

}  // namespace remoting::mac

#endif  // REMOTING_HOST_MAC_PERMISSION_UTILS_H_
