// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/preferences/preference_manager.h"
#include "third_party/blink/renderer/core/preferences/preference_object.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

PreferenceManager::PreferenceManager(ExecutionContext* executionContext) {
  color_scheme_ = MakeGarbageCollected<PreferenceObject>(
      executionContext, AtomicString("colorScheme"));
  contrast_ = MakeGarbageCollected<PreferenceObject>(executionContext,
                                                     AtomicString("contrast"));
  reduced_motion_ = MakeGarbageCollected<PreferenceObject>(
      executionContext, AtomicString("reducedMotion"));
  reduced_transparency_ = MakeGarbageCollected<PreferenceObject>(
      executionContext, AtomicString("reducedTransparency"));
  reduced_data_ = MakeGarbageCollected<PreferenceObject>(
      executionContext, AtomicString("reducedData"));
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

}  // namespace blink
