// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/preferences/preference_object.h"

#include "third_party/blink/renderer/bindings/core/v8/frozen_array.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/css/media_values.h"
#include "third_party/blink/renderer/core/css/media_values_dynamic.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/preferences/preference_overrides.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

AtomicString ColorSchemeToString(
    mojom::blink::PreferredColorScheme colorScheme) {
  switch (colorScheme) {
    case mojom::PreferredColorScheme::kLight:
      return AtomicString("light");
    case mojom::PreferredColorScheme::kDark:
      return AtomicString("dark");
    default:
      NOTREACHED();
      return g_empty_atom;
  }
}

AtomicString ContrastToString(mojom::blink::PreferredContrast contrast) {
  switch (contrast) {
    case mojom::PreferredContrast::kMore:
      return AtomicString("more");
    case mojom::PreferredContrast::kLess:
      return AtomicString("less");
    case mojom::PreferredContrast::kCustom:
      return AtomicString("custom");
    case mojom::PreferredContrast::kNoPreference:
      return AtomicString("no-preference");
    default:
      NOTREACHED();
      return g_empty_atom;
  }
}

PreferenceObject::PreferenceObject(ExecutionContext* executionContext,
                                   AtomicString name)
    : name_(name) {
  LocalFrame* frame = nullptr;
  if (executionContext && !executionContext->IsContextDestroyed()) {
    frame = DynamicTo<LocalDOMWindow>(executionContext)->GetFrame();
  }
  media_values_ = MediaValues::CreateDynamicIfFrameExists(frame);
}

PreferenceObject::~PreferenceObject() = default;

std::optional<AtomicString> PreferenceObject::override(
    ScriptState* script_state) {
  if (!script_state || !script_state->ContextIsValid()) {
    return std::nullopt;
  }
  auto* execution_context = ExecutionContext::From(script_state);
  if (!execution_context || execution_context->IsContextDestroyed()) {
    return std::nullopt;
  }
  auto* window = DynamicTo<LocalDOMWindow>(execution_context);
  if (!window) {
    return std::nullopt;
  }

  const PreferenceOverrides* overrides =
      window->GetFrame()->GetPage()->GetPreferenceOverrides();

  if (!overrides) {
    return std::nullopt;
  }

  if (name_ == "colorScheme") {
    std::optional<mojom::blink::PreferredColorScheme> color_scheme =
        overrides->GetPreferredColorScheme();
    if (!color_scheme.has_value()) {
      return std::nullopt;
    }

    return std::make_optional(ColorSchemeToString(color_scheme.value()));
  } else if (name_ == "contrast") {
    std::optional<mojom::blink::PreferredContrast> contrast =
        overrides->GetPreferredContrast();
    if (!contrast.has_value()) {
      return std::nullopt;
    }

    return std::make_optional(ContrastToString(contrast.value()));
  } else if (name_ == "reducedMotion") {
    std::optional<bool> reduced_motion = overrides->GetPrefersReducedMotion();
    if (!reduced_motion.has_value()) {
      return std::nullopt;
    }

    return std::make_optional(
        AtomicString(reduced_motion.value() ? "reduce" : "no-preference"));
  } else if (name_ == "reducedTransparency") {
    std::optional<bool> reduced_transparency =
        overrides->GetPrefersReducedTransparency();
    if (!reduced_transparency.has_value()) {
      return std::nullopt;
    }

    return std::make_optional(AtomicString(
        reduced_transparency.value() ? "reduce" : "no-preference"));
  } else if (name_ == "reducedData") {
    std::optional<bool> reduced_data = overrides->GetPrefersReducedData();
    if (!reduced_data.has_value()) {
      return std::nullopt;
    }

    return std::make_optional(
        AtomicString(reduced_data.value() ? "reduce" : "no-preference"));
  } else {
    NOTREACHED();
    return std::nullopt;
  }
}

AtomicString PreferenceObject::value(ScriptState* script_state) {
  if (!script_state || !script_state->ContextIsValid()) {
    return g_empty_atom;
  }
  auto* execution_context = ExecutionContext::From(script_state);
  if (!execution_context || execution_context->IsContextDestroyed()) {
    return g_empty_atom;
  }
  auto* window = DynamicTo<LocalDOMWindow>(execution_context);
  if (!window) {
    return g_empty_atom;
  }

  if (name_ == "colorScheme") {
    mojom::blink::PreferredColorScheme color_scheme =
        media_values_->GetPreferredColorScheme();
    return ColorSchemeToString(color_scheme);
  } else if (name_ == "contrast") {
    mojom::blink::PreferredContrast contrast =
        media_values_->GetPreferredContrast();
    return ContrastToString(contrast);
  } else if (name_ == "reducedMotion") {
    bool reduced_motion = media_values_->PrefersReducedMotion();
    return AtomicString(reduced_motion ? "reduce" : "no-preference");
  } else if (name_ == "reducedTransparency") {
    bool reduced_transparency = media_values_->PrefersReducedTransparency();
    return AtomicString(reduced_transparency ? "reduce" : "no-preference");
  } else if (name_ == "reducedData") {
    bool reduced_data = media_values_->PrefersReducedData();
    return AtomicString(reduced_data ? "reduce" : "no-preference");
  } else {
    NOTREACHED();
    return g_empty_atom;
  }
}

void PreferenceObject::clearOverride(ScriptState* script_state) {
  if (!script_state || !script_state->ContextIsValid()) {
    return;
  }
  auto* execution_context = ExecutionContext::From(script_state);
  if (!execution_context || execution_context->IsContextDestroyed()) {
    return;
  }
  auto* window = DynamicTo<LocalDOMWindow>(execution_context);
  if (!window) {
    return;
  }

  const PreferenceOverrides* overrides =
      window->GetFrame()->GetPage()->GetPreferenceOverrides();

  if (!overrides) {
    return;
  }

  AtomicString featureName;
  if (name_ == "colorScheme") {
    std::optional<mojom::blink::PreferredColorScheme> color_scheme =
        overrides->GetPreferredColorScheme();
    if (!color_scheme.has_value()) {
      return;
    }

    featureName = AtomicString("prefers-color-scheme");
  } else if (name_ == "contrast") {
    std::optional<mojom::blink::PreferredContrast> contrast =
        overrides->GetPreferredContrast();
    if (!contrast.has_value()) {
      return;
    }

    featureName = AtomicString("prefers-contrast");
  } else if (name_ == "reducedMotion") {
    std::optional<bool> reduced_motion = overrides->GetPrefersReducedMotion();
    if (!reduced_motion.has_value()) {
      return;
    }

    featureName = AtomicString("prefers-reduced-motion");
  } else if (name_ == "reducedTransparency") {
    std::optional<bool> reduced_transparency =
        overrides->GetPrefersReducedTransparency();
    if (!reduced_transparency.has_value()) {
      return;
    }

    featureName = AtomicString("prefers-reduced-transparency");
  } else if (name_ == "reducedData") {
    std::optional<bool> reduced_data = overrides->GetPrefersReducedData();
    if (!reduced_data.has_value()) {
      return;
    }

    featureName = AtomicString("prefers-reduced-data");
  } else {
    NOTREACHED();
  }
  window->GetFrame()->GetPage()->SetPreferenceOverride(featureName, String());
}

ScriptPromise PreferenceObject::requestOverride(
    ScriptState* script_state,
    std::optional<AtomicString> value) {
  if (!script_state || !script_state->ContextIsValid()) {
    return ScriptPromise();
  }
  auto* execution_context = ExecutionContext::From(script_state);
  if (!execution_context || execution_context->IsContextDestroyed()) {
    return ScriptPromise();
  }
  auto* window = DynamicTo<LocalDOMWindow>(execution_context);
  if (!window) {
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (!value.has_value() || value.value().empty()) {
    clearOverride(script_state);
    resolver->Resolve();

    return promise;
  }

  // TODO add equality check before overriding

  AtomicString featureName;
  String newValue;

  if (name_ == "colorScheme") {
    featureName = AtomicString("prefers-color-scheme");

    if (value == "light") {
      newValue = "light";
    } else if (value == "dark") {
      newValue = "dark";
    }
  } else if (name_ == "contrast") {
    featureName = AtomicString("prefers-contrast");

    if (value == "more") {
      newValue = "more";
    } else if (value == "less") {
      newValue = "less";
    } else if (value == "no-preference") {
      newValue = "no-preference";
    }
  } else if (name_ == "reducedMotion") {
    featureName = AtomicString("prefers-reduced-motion");

    if (value == "reduce") {
      newValue = "reduce";
    } else if (value == "no-preference") {
      newValue = "no-preference";
    }
  } else if (name_ == "reducedTransparency") {
    featureName = AtomicString("prefers-reduced-transparency");

    if (value == "reduce") {
      newValue = "reduce";
    } else if (value == "no-preference") {
      newValue = "no-preference";
    }
  } else if (name_ == "reducedData") {
    featureName = AtomicString("prefers-reduced-data");

    if (value == "reduce") {
      newValue = "reduce";
    } else if (value == "no-preference") {
      newValue = "no-preference";
    }
  } else {
    NOTREACHED();
  }

  if (newValue.empty()) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kTypeMismatchError,
        value.value() + " is not a valid value."));
    return promise;
  }

  window->GetFrame()->GetPage()->SetPreferenceOverride(featureName, newValue);
  resolver->Resolve();

  return promise;
}

const FrozenArray<IDLString>& PreferenceObject::validValues() {
  if (LIKELY(valid_values_)) {
    return *valid_values_.Get();
  }

  FrozenArray<IDLString>::VectorType valid_values;
  if (name_ == "colorScheme") {
    valid_values.push_back("light");
    valid_values.push_back("dark");
  } else if (name_ == "contrast") {
    valid_values.push_back("more");
    valid_values.push_back("less");
    valid_values.push_back("no-preference");
  } else if (name_ == "reducedMotion") {
    valid_values.push_back("reduce");
    valid_values.push_back("no-preference");
  } else if (name_ == "reducedTransparency") {
    valid_values.push_back("reduce");
    valid_values.push_back("no-preference");
  } else if (name_ == "reducedData") {
    valid_values.push_back("reduce");
    valid_values.push_back("no-preference");
  } else {
    NOTREACHED();
  }
  valid_values_ =
      MakeGarbageCollected<FrozenArray<IDLString>>(std::move(valid_values));
  return *valid_values_.Get();
}

void PreferenceObject::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(valid_values_);
  visitor->Trace(media_values_);
}

}  // namespace blink
