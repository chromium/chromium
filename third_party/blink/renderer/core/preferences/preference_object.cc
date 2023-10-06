// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/preferences/preference_object.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
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

PreferenceObject::PreferenceObject(AtomicString name) : name_(name) {}

PreferenceObject::~PreferenceObject() = default;

absl::optional<AtomicString> PreferenceObject::override(
    ScriptState* script_state) {
  if (!script_state || !script_state->ContextIsValid()) {
    return absl::nullopt;
  }
  auto* execution_context = ExecutionContext::From(script_state);
  if (!execution_context || execution_context->IsContextDestroyed()) {
    return absl::nullopt;
  }
  auto* window = DynamicTo<LocalDOMWindow>(execution_context);
  if (!window) {
    return absl::nullopt;
  }

  const PreferenceOverrides* overrides =
      window->GetFrame()->GetPage()->GetPreferenceOverrides();

  if (!overrides) {
    return absl::nullopt;
  }

  if (name_ == "colorScheme") {
    absl::optional<mojom::blink::PreferredColorScheme> color_scheme =
        overrides->GetPreferredColorScheme();
    if (!color_scheme.has_value()) {
      return absl::nullopt;
    }

    switch (color_scheme.value()) {
      case mojom::PreferredColorScheme::kLight:
        return absl::make_optional(AtomicString("light"));
      case mojom::PreferredColorScheme::kDark:
        return absl::make_optional(AtomicString("dark"));
      default:
        NOTREACHED();
        return absl::nullopt;
    }
  } else if (name_ == "contrast") {
    absl::optional<mojom::blink::PreferredContrast> contrast =
        overrides->GetPreferredContrast();
    if (!contrast.has_value()) {
      return absl::nullopt;
    }

    switch (contrast.value()) {
      case mojom::PreferredContrast::kMore:
        return absl::make_optional(AtomicString("more"));
      case mojom::PreferredContrast::kLess:
        return absl::make_optional(AtomicString("less"));
      default:
        NOTREACHED();
        return absl::nullopt;
    }
  } else if (name_ == "reducedMotion") {
    absl::optional<bool> reduced_motion = overrides->GetPrefersReducedMotion();
    if (!reduced_motion.has_value()) {
      return absl::nullopt;
    }

    return absl::make_optional(AtomicString("reduce"));
  } else if (name_ == "reducedTransparency") {
    absl::optional<bool> reduced_transparency =
        overrides->GetPrefersReducedTransparency();
    if (!reduced_transparency.has_value()) {
      return absl::nullopt;
    }

    return absl::make_optional(AtomicString("reduce"));
  } else if (name_ == "reducedData") {
    absl::optional<bool> reduced_data = overrides->GetPrefersReducedData();
    if (!reduced_data.has_value()) {
      return absl::nullopt;
    }

    return absl::make_optional(AtomicString("reduce"));
  } else {
    NOTREACHED();
    return absl::nullopt;
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
    absl::optional<mojom::blink::PreferredColorScheme> color_scheme =
        overrides->GetPreferredColorScheme();
    if (!color_scheme.has_value()) {
      return;
    }

    featureName = AtomicString("prefers-color-scheme");
  } else if (name_ == "contrast") {
    absl::optional<mojom::blink::PreferredContrast> contrast =
        overrides->GetPreferredContrast();
    if (!contrast.has_value()) {
      return;
    }

    featureName = AtomicString("prefers-contrast");
  } else if (name_ == "reducedMotion") {
    absl::optional<bool> reduced_motion = overrides->GetPrefersReducedMotion();
    if (!reduced_motion.has_value()) {
      return;
    }

    featureName = AtomicString("prefers-reduced-motion");
  } else if (name_ == "reducedTransparency") {
    absl::optional<bool> reduced_transparency =
        overrides->GetPrefersReducedTransparency();
    if (!reduced_transparency.has_value()) {
      return;
    }

    featureName = AtomicString("prefers-reduced-transparency");
  } else if (name_ == "reducedData") {
    absl::optional<bool> reduced_data = overrides->GetPrefersReducedData();
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
    absl::optional<AtomicString> value) {
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
    }
  } else if (name_ == "reducedMotion") {
    featureName = AtomicString("prefers-reduced-motion");

    if (value == "reduce") {
      newValue = "reduce";
    }
  } else if (name_ == "reducedTransparency") {
    featureName = AtomicString("prefers-reduced-transparency");

    if (value == "reduce") {
      newValue = "reduce";
    }
  } else if (name_ == "reducedData") {
    featureName = AtomicString("prefers-reduced-data");

    if (value == "reduce") {
      newValue = "reduce";
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

Vector<AtomicString> PreferenceObject::validValues() {
  Vector<AtomicString> valid_values;
  if (name_ == "colorScheme") {
    valid_values.push_back("light");
    valid_values.push_back("dark");
  } else if (name_ == "contrast") {
    valid_values.push_back("more");
    valid_values.push_back("less");
  } else if (name_ == "reducedMotion") {
    valid_values.push_back("reduce");
  } else if (name_ == "reducedTransparency") {
    valid_values.push_back("reduce");
  } else if (name_ == "reducedData") {
    valid_values.push_back("reduce");
  } else {
    NOTREACHED();
  }
  return valid_values;
}

}  // namespace blink
