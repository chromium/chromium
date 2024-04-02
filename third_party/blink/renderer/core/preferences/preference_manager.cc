// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/preferences/preference_manager.h"
#include "third_party/blink/renderer/core/preferences/preference_names.h"
#include "third_party/blink/renderer/core/preferences/preference_object.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

PreferenceManager::PreferenceManager(ExecutionContext* executionContext) {
  color_scheme_ = MakeGarbageCollected<PreferenceObject>(
      executionContext, preference_names::kColorScheme);
  contrast_ = MakeGarbageCollected<PreferenceObject>(
      executionContext, preference_names::kContrast);
  reduced_motion_ = MakeGarbageCollected<PreferenceObject>(
      executionContext, preference_names::kReducedMotion);
  reduced_transparency_ = MakeGarbageCollected<PreferenceObject>(
      executionContext, preference_names::kReducedTransparency);
  reduced_data_ = MakeGarbageCollected<PreferenceObject>(
      executionContext, preference_names::kReducedData);
}

PreferenceManager::~PreferenceManager() = default;

void PreferenceManager::Trace(Visitor* visitor) const {
  visitor->Trace(color_scheme_);
  visitor->Trace(contrast_);
  visitor->Trace(reduced_motion_);
  visitor->Trace(reduced_transparency_);
  visitor->Trace(reduced_data_);
  ScriptWrappable::Trace(visitor);
}

PreferenceObject* PreferenceManager::colorScheme() {
  return color_scheme_.Get();
}

PreferenceObject* PreferenceManager::contrast() {
  return contrast_.Get();
}

PreferenceObject* PreferenceManager::reducedMotion() {
  return reduced_motion_.Get();
}

PreferenceObject* PreferenceManager::reducedTransparency() {
  return reduced_transparency_.Get();
}

PreferenceObject* PreferenceManager::reducedData() {
  return reduced_data_.Get();
}

void PreferenceManager::PreferenceMaybeChanged() {
  if (!RuntimeEnabledFeatures::WebPreferencesEnabled()) {
    return;
  }

  colorScheme()->PreferenceMaybeChanged();
  contrast()->PreferenceMaybeChanged();
  reducedMotion()->PreferenceMaybeChanged();
  reducedTransparency()->PreferenceMaybeChanged();
  reducedData()->PreferenceMaybeChanged();
}

}  // namespace blink
