// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PRIVACY_BUDGET_IDENTIFIABILITY_STUDY_SETTINGS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PRIVACY_BUDGET_IDENTIFIABILITY_STUDY_SETTINGS_H_

#include <memory>

#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings_provider.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

namespace blink {

// Determines whether the identifiability study is active and if so whether a
// given surface or surface type should be sampled.
//
// This class can used from multiple threads and does not require
// synchronization.
//
// See documentation on individual methods for notes on thread safety.
//
// Guidelines for when and how to use it can be found in:
//
//     //docs/privacy_budget_instrumentation.md#gating
//
class BLINK_COMMON_EXPORT IdentifiabilityStudySettings {
 public:
  // Constructs a default IdentifiabilityStudySettings instance. By default the
  // settings instance acts as if the study is disabled, and implicitly as if
  // all surfaces and types are blocked.
  IdentifiabilityStudySettings();

  // Constructs a IdentifiabilityStudySettings instance which reflects the state
  // specified by |provider|.
  explicit IdentifiabilityStudySettings(
      std::unique_ptr<IdentifiabilityStudySettingsProvider> provider);

  ~IdentifiabilityStudySettings();

  // Get a pointer to an instance of IdentifiabilityStudySettings for the
  // process.
  //
  // This method and the returned object is safe to use from any thread and is
  // never destroyed.
  //
  // On the browser process, the returned instance is authoritative. On all
  // other processes the returned instance should be considered advisory. It's
  // only meant as an optimization to avoid calculating things unnecessarily.
  static const IdentifiabilityStudySettings* Get();

  // Initialize the process-wide settings instance with the specified settings
  // provider. Should only be called once per process and only from the main
  // thread.
  //
  // For testing, you can use ResetStateForTesting().
  static void SetGlobalProvider(
      std::unique_ptr<IdentifiabilityStudySettingsProvider> provider);

  // Returns true if the study is active for this client. Once if it returns
  // true, it doesn't return false at any point after. The converse is not true.
  bool IsActive() const;

  // Returns true if |surface| is allowed.
  //
  // Will always return false if IsActive() is false. I.e. If the study is
  // inactive, all surfaces are considered to be blocked. Hence it is sufficient
  // to call this function directly instead of calling IsActive() before it.
  bool IsSurfaceAllowed(IdentifiableSurface surface) const;

  // Returns true if |type| is allowed.
  //
  // Will always return false if IsActive() is false. I.e. If the study is
  // inactive, all surface types are considered to be blocked. Hence it is
  // sufficient to call this function directly instead of calling IsActive()
  // before it.
  bool IsTypeAllowed(IdentifiableSurface::Type type) const;

  // Only used for testing. Resets internal state and violates API contracts
  // made above about the lifetime of IdentifiabilityStudySettings*.
  static void ResetStateForTesting();

  IdentifiabilityStudySettings(IdentifiabilityStudySettings&&) = delete;
  IdentifiabilityStudySettings(const IdentifiabilityStudySettings&) = delete;
  IdentifiabilityStudySettings& operator=(const IdentifiabilityStudySettings&) =
      delete;

 private:
  const std::unique_ptr<IdentifiabilityStudySettingsProvider> provider_;
  const bool is_enabled_ = false;
  const bool is_any_surface_or_type_blocked_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PRIVACY_BUDGET_IDENTIFIABILITY_STUDY_SETTINGS_H_
