// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/supported_cdm_versions.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/media_switches.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media {

namespace {

// Returns the overridden supported CDM interface version specified on command
// line, which can be null if not specified.
absl::optional<int> GetSupportedCdmInterfaceVersionOverrideFromCommandLine() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line)
    return absl::nullopt;

  auto version_string = command_line->GetSwitchValueASCII(
      switches::kOverrideEnabledCdmInterfaceVersion);

  int version = 0;
  if (!base::StringToInt(version_string, &version))
    return absl::nullopt;
  else
    return version;
}

}  // namespace

bool IsSupportedAndEnabledCdmInterfaceVersion(int version) {
  if (!IsSupportedCdmInterfaceVersion(version))
    return false;

  auto version_override =
      GetSupportedCdmInterfaceVersionOverrideFromCommandLine();
  if (version_override)
    return version == version_override;

  return IsCdmInterfaceVersionEnabledByDefault(version);
}

}  // namespace media
