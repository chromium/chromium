// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PREFERENCES_PREFERENCE_OBJECT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PREFERENCES_PREFERENCE_OBJECT_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

// Spec: https://wicg.github.io/web-preferences-api/#preferenceobject-interface
class PreferenceObject final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit PreferenceObject(AtomicString name);

  ~PreferenceObject() override;

  absl::optional<AtomicString> override(ScriptState*);

  void clearOverride(ScriptState*);

  ScriptPromise requestOverride(ScriptState*, absl::optional<AtomicString>);

  Vector<AtomicString> validValues();

 private:
  AtomicString name_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PREFERENCES_PREFERENCE_OBJECT_H_
