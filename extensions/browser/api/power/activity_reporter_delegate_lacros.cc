// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/power/activity_reporter_delegate_lacros.h"

#include "base/values.h"
#include "chromeos/crosapi/mojom/power.mojom.h"
#include "chromeos/lacros/lacros_service.h"

namespace {

const char kUnsupportedByAsh[] = "Unsupported by ash";

crosapi::mojom::Power* GetPowerApi() {
  return chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::Power>()
      .get();
}

// Performs common crosapi validation. These errors are not caused by the
// extension so they are considered recoverable. Returns an error message on
// error, or nullopt on success.
// |min_version| is the minimum version of the ash implementation of
// crosapi::mojom::Power necessary to run a specific API method.
absl::optional<std::string> ValidateCrosapi(int min_version) {
  if (!chromeos::LacrosService::Get()->IsAvailable<crosapi::mojom::Power>()) {
    return kUnsupportedByAsh;
  }

  int interface_version = chromeos::LacrosService::Get()->GetInterfaceVersion(
      crosapi::mojom::Power::Uuid_);
  if (interface_version < min_version) {
    return kUnsupportedByAsh;
  }

  return absl::nullopt;
}

}  // namespace

namespace extensions {

ActivityReporterDelegateLacros::ActivityReporterDelegateLacros() = default;

ActivityReporterDelegateLacros::~ActivityReporterDelegateLacros() = default;

absl::optional<std::string> ActivityReporterDelegateLacros::ReportActivity()
    const {
  absl::optional<std::string> error =
      ValidateCrosapi(crosapi::mojom::Power::kReportActivityMinVersion);
  if (error.has_value()) {
    return error;
  }
  GetPowerApi()->ReportActivity();
  return absl::nullopt;
}

}  // namespace extensions
