// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/win/mf_feature_checks.h"

#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "media/base/media_switches.h"

namespace media {

bool SupportMediaFoundationPlayback() {
  return SupportMediaFoundationClearPlayback() ||
         SupportMediaFoundationEncryptedPlayback();
}

bool SupportMediaFoundationClearPlayback() {
  return base::win::GetVersion() >= base::win::Version::WIN10_RS3 &&
         base::FeatureList::IsEnabled(kMediaFoundationClearPlayback);
}

bool SupportMediaFoundationEncryptedPlayback() {
#if defined(ARCH_CPU_X86_64)
  // MediaFoundation encrypted playback is supported only on x86_64 (AMD64
  // Windows) while Chrome bitness matches OS bitness. We won't let a x86_64
  // Chrome build be running on ARM64 with the WOW emulation.
  auto is_running_emulated_on_arm64 =
      base::win::OSInfo::IsRunningEmulatedOnArm64();
  DVLOG(3) << __func__
           << ": is_running_emulated_on_arm64=" << is_running_emulated_on_arm64;

  // TODO(xhwang): Also check whether software secure decryption is enabled and
  // supported by MediaFoundationCdm in the future.
  return base::win::GetVersion() >= base::win::Version::WIN10_20H1 &&
         !is_running_emulated_on_arm64 && IsHardwareSecureDecryptionEnabled();
#else
  return false;
#endif  // defined(ARCH_CPU_X86_64)
}

}  // namespace media
