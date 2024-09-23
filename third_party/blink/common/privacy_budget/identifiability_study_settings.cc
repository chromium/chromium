// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"

#include <initializer_list>
#include <optional>
#include <random>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/no_destructor.h"
#include "base/synchronization/atomic_flag.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings_provider.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

namespace blink {

namespace {

bool IdentifiabilityTracingEnabled() {
  bool tracing_enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("identifiability"), &tracing_enabled);
  return tracing_enabled;
}

// IdentifiabilityStudySettings is meant to be used as a global singleton. Its
// use is subject to the following constraints.
//
//   1. The embedder should be able to set the
//      IdentifiabilityStudySettingsProvider at any point during execution. This
//      relaxation allows the embedder to perform any required initialization
//      without blocking process startup.
//
//   2. Get() and the returned IdentifiabilityStudySettings instance should be
//      usable from any thread. The returned object must always be well
//      formed with an infinite lifetime.
//
//   3. Calling Get() "prior" to the embedder calling SetProvider() should be
//      harmless and non-blocking.
//
//   4. Be fast.
class ThreadsafeSettingsWrapper {
 public:
  ThreadsafeSettingsWrapper() = default;

  const IdentifiabilityStudySettings* GetSettings() {
    // Access to initialized_settings_ is behind a memory barrier used for
    // accessing the atomic flag |initialized_|. The state of
    // |initialized_settings_| is consistent due to the acquire-release
    // semantics enforced by |AtomicFlag|. I.e. writes prior to
    // AtomicFlag::Set() is visible after a AtomicFlag::IsSet() which returns
    // true.
    //
    // If the flag is not set, then |default_settings_| can be used instead.
    //
    // In either case, the returned pointer...
    //   1. ... Points to a well formed IdentifiabilityStudySettings object.
    //   2. ... Is valid for the remainder of the process lifetime.
    //   3. ... Is safe to use from any thread.
    if (!initialized_.IsSet())
      return &default_settings_;
    return &initialized_settings_.value();
  }

  // Same restrictions as IdentifiabilityStudySettings::SetGlobalProvider().
  void SetProvider(
      std::unique_ptr<IdentifiabilityStudySettingsProvider> provider) {
    DCHECK(!initialized_.IsSet());
    initialized_settings_.emplace(std::move(provider));
    initialized_.Set();
  }

  void ResetStateForTesting() {
    initialized_settings_.reset();
    initialized_.UnsafeResetForTesting();
  }

  // Function local static initializer is initialized in a threadsafe manner.
  // This object itself is cheap to construct.
  static ThreadsafeSettingsWrapper* GetWrapper() {
    static base::NoDestructor<ThreadsafeSettingsWrapper> wrapper;
    return wrapper.get();
  }

 private:
  std::optional<IdentifiabilityStudySettings> initialized_settings_;
  const IdentifiabilityStudySettings default_settings_;
  base::AtomicFlag initialized_;
};

}  // namespace

IdentifiabilityStudySettingsProvider::~IdentifiabilityStudySettingsProvider() =
    default;

IdentifiabilityStudySettings::IdentifiabilityStudySettings() = default;

IdentifiabilityStudySettings::IdentifiabilityStudySettings(
    std::unique_ptr<IdentifiabilityStudySettingsProvider> provider)
    : provider_(std::move(provider)),
      is_enabled_(provider_->IsActive()),
      is_any_surface_or_type_blocked_(provider_->IsAnyTypeOrSurfaceBlocked()),
      is_meta_experiment_active_(provider_->IsMetaExperimentActive()) {}

IdentifiabilityStudySettings::~IdentifiabilityStudySettings() = default;

// static
const IdentifiabilityStudySettings* IdentifiabilityStudySettings::Get() {
  return ThreadsafeSettingsWrapper::GetWrapper()->GetSettings();
}

// static
void IdentifiabilityStudySettings::SetGlobalProvider(
    std::unique_ptr<IdentifiabilityStudySettingsProvider> provider) {
  ThreadsafeSettingsWrapper::GetWrapper()->SetProvider(std::move(provider));
}

void IdentifiabilityStudySettings::ResetStateForTesting() {
  ThreadsafeSettingsWrapper::GetWrapper()->ResetStateForTesting();
}

bool IdentifiabilityStudySettings::IsActive() const {
  return is_enabled_ || is_meta_experiment_active_;
}

bool IdentifiabilityStudySettings::ShouldSampleWebFeature(
    mojom::WebFeature feature) const {
  return ShouldSampleSurface(IdentifiableSurface::FromTypeAndToken(
      IdentifiableSurface::Type::kWebFeature, feature));
}

bool IdentifiabilityStudySettings::ShouldSampleSurface(
    IdentifiableSurface surface) const {
  if (!ShouldSampleAnything()) [[likely]] {
    return false;
  }

  if (!is_any_surface_or_type_blocked_) [[likely]] {
    return true;
  }

  if (is_meta_experiment_active_) {
    return true;
  }

  return provider_->IsSurfaceAllowed(surface);
}

bool IdentifiabilityStudySettings::ShouldSampleType(
    IdentifiableSurface::Type type) const {
  if (!ShouldSampleAnything()) [[likely]] {
    return false;
  }

  if (!is_any_surface_or_type_blocked_) [[likely]] {
    return true;
  }

  if (is_meta_experiment_active_) {
    return true;
  }

  return provider_->IsTypeAllowed(type);
}

bool IdentifiabilityStudySettings::ShouldSampleAnyType(
    std::initializer_list<IdentifiableSurface::Type> types) const {
  if (!ShouldSampleAnything()) [[likely]] {
    return false;
  }

  if (!is_any_surface_or_type_blocked_) [[likely]] {
    return true;
  }

  if (is_meta_experiment_active_) {
    return true;
  }

  for (IdentifiableSurface::Type type : types) {
    if (provider_->IsTypeAllowed(type))
      return true;
  }

  return false;
}

bool IdentifiabilityStudySettings::ShouldSampleAnything() const {
  return IsActive() || IdentifiabilityTracingEnabled();
}

}  // namespace blink
