// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/audio_sandbox_win.h"

#include "sandbox/win/src/sandbox_policy.h"

// NOTE: changes to this code need to be reviewed by the security team.

namespace audio {

//------------------------------------------------------------------------------
// Public audio service sandbox configuration extension functions.
//------------------------------------------------------------------------------
//
//  Default policy:
//
//  lockdown_level_(sandbox::USER_LOCKDOWN),
//  initial_level_(sandbox::USER_RESTRICTED_SAME_ACCESS),
//
//  job_level_(sandbox::JOB_LOCKDOWN),
//
//  integrity_level_(sandbox::INTEGRITY_LEVEL_LOW),
//  delayed_integrity_level_(sandbox::INTEGRITY_LEVEL_UNTRUSTED),

bool AudioPreSpawnTarget(sandbox::TargetPolicy* policy) {
  // Audio process privilege requirements:
  //  - Lockdown level of USER_NON_ADMIN
  //  - Delayed integrity level of INTEGRITY_LEVEL_LOW
  //
  // For audio streams to create shared memory regions, lockdown level must be
  // at least USER_LIMITED and delayed integrity level INTEGRITY_LEVEL_LOW,
  // otherwise CreateFileMapping() will fail with error code ERROR_ACCESS_DENIED
  // (0x5).
  //
  // For audio input streams to use ISimpleAudioVolume interface, lockdown
  // level must be set to USER_NON_ADMIN, otherwise
  // WASAPIAudioInputStream::Open() will fail with error code E_ACCESSDENIED
  // (0x80070005) when trying to get a reference to ISimpleAudioVolume
  // interface. See
  // https://cs.chromium.org/chromium/src/media/audio/win/audio_low_latency_input_win.cc
  // Use USER_RESTRICTED_NON_ADMIN over USER_NON_ADMIN to prevent failures when
  // AppLocker and similar application whitelisting solutions are in place.
  policy->SetTokenLevel(sandbox::USER_RESTRICTED_SAME_ACCESS,
                        sandbox::USER_RESTRICTED_NON_ADMIN);
  policy->SetDelayedIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);

  // Custom default policy allowing audio drivers to read device properties
  // (https://crbug.com/883326).
  policy->SetIntegrityLevel(sandbox::INTEGRITY_LEVEL_LOW);
  policy->SetLockdownDefaultDacl();
  policy->SetAlternateDesktop(true);

  return true;
}

}  // namespace audio
