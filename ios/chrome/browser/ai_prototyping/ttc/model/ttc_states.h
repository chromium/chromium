// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_STATES_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_STATES_H_

// High-level service availability and active session state for a Profile.
enum class TTCServiceState {
  // TTC is unavailable or disabled for this profile.
  kProfileIneligible,
  // TTC is available but no session is currently active.
  kSessionInactive,
  // A voice session is currently in progress.
  kSessionActive,
};

// The lifecycle state of a single TTC session.
enum class TTCSessionLifecycle {
  // The session has been created but the backend is not yet connected.
  kInitializing,
  // The backend is connected and the session is interactive.
  kLive,
  // The session has been disconnected and is no longer usable.
  kFinished,
};

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_STATES_H_
