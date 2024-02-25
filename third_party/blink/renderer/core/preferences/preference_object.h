// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PREFERENCES_PREFERENCE_OBJECT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PREFERENCES_PREFERENCE_OBJECT_H_

#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

template <typename IDLType>
class FrozenArray;
class MediaValues;

// Spec: https://wicg.github.io/web-preferences-api/#preferenceobject-interface
class PreferenceObject final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  PreferenceObject(ExecutionContext*, AtomicString name);

  ~PreferenceObject() override;

  std::optional<AtomicString> override(ScriptState*);

  AtomicString value(ScriptState*);

  void clearOverride(ScriptState*);

  ScriptPromise requestOverride(ScriptState*, std::optional<AtomicString>);

  const FrozenArray<IDLString>& validValues();

  void Trace(Visitor* visitor) const override;

 private:
  AtomicString name_;
  Member<FrozenArray<IDLString>> valid_values_;
  Member<const MediaValues> media_values_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PREFERENCES_PREFERENCE_OBJECT_H_
