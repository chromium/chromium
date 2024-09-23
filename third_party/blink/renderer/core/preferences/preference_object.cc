// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/preferences/preference_object.h"

#include "third_party/blink/renderer/bindings/core/v8/frozen_array.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/css/media_values.h"
#include "third_party/blink/renderer/core/css/media_values_cached.h"
#include "third_party/blink/renderer/core/css/media_values_dynamic.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/preferences/preference_names.h"
#include "third_party/blink/renderer/core/preferences/preference_overrides.h"
#include "third_party/blink/renderer/core/preferences/preference_values.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

AtomicString ColorSchemeToString(
    mojom::blink::PreferredColorScheme colorScheme) {
  switch (colorScheme) {
    case mojom::PreferredColorScheme::kLight:
      return preference_values::kLight;
    case mojom::PreferredColorScheme::kDark:
      return preference_values::kDark;
    default:
      NOTREACHED_IN_MIGRATION();
      return g_empty_atom;
  }
}

AtomicString ContrastToString(mojom::blink::PreferredContrast contrast) {
  switch (contrast) {
    case mojom::PreferredContrast::kMore:
      return preference_values::kMore;
    case mojom::PreferredContrast::kLess:
      return preference_values::kLess;
    case mojom::PreferredContrast::kCustom:
      return preference_values::kCustom;
    case mojom::PreferredContrast::kNoPreference:
      return preference_values::kNoPreference;
    default:
      NOTREACHED_IN_MIGRATION();
      return g_empty_atom;
  }
}

PreferenceObject::PreferenceObject(ExecutionContext* executionContext,
                                   AtomicString name)
    : ExecutionContextLifecycleObserver(executionContext), name_(name) {
  LocalFrame* frame = nullptr;
  if (executionContext && !executionContext->IsContextDestroyed()) {
    frame = DynamicTo<LocalDOMWindow>(executionContext)->GetFrame();
  }
  media_values_ = MediaValues::CreateDynamicIfFrameExists(frame);
  preferred_color_scheme_ = media_values_->GetPreferredColorScheme();
  preferred_contrast_ = media_values_->GetPreferredContrast();
  prefers_reduced_data_ = media_values_->PrefersReducedData();
  prefers_reduced_motion_ = media_values_->PrefersReducedMotion();
  prefers_reduced_transparency_ = media_values_->PrefersReducedTransparency();
}

PreferenceObject::~PreferenceObject() = default;

std::optional<AtomicString> PreferenceObject::override(
    ScriptState* script_state) {
  CHECK(RuntimeEnabledFeatures::WebPreferencesEnabled());
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

  if (name_ == preference_names::kColorScheme) {
    std::optional<mojom::blink::PreferredColorScheme> color_scheme =
        overrides->GetPreferredColorScheme();
    if (!color_scheme.has_value()) {
      return std::nullopt;
    }

    return std::make_optional(ColorSchemeToString(color_scheme.value()));
  } else if (name_ == preference_names::kContrast) {
    std::optional<mojom::blink::PreferredContrast> contrast =
        overrides->GetPreferredContrast();
    if (!contrast.has_value()) {
      return std::nullopt;
    }

    return std::make_optional(ContrastToString(contrast.value()));
  } else if (name_ == preference_names::kReducedMotion) {
    std::optional<bool> reduced_motion = overrides->GetPrefersReducedMotion();
    if (!reduced_motion.has_value()) {
      return std::nullopt;
    }

    return std::make_optional(reduced_motion.value()
                                  ? preference_values::kReduce
                                  : preference_values::kNoPreference);
  } else if (name_ == preference_names::kReducedTransparency) {
    std::optional<bool> reduced_transparency =
        overrides->GetPrefersReducedTransparency();
    if (!reduced_transparency.has_value()) {
      return std::nullopt;
    }

    return std::make_optional(reduced_transparency.value()
                                  ? preference_values::kReduce
                                  : preference_values::kNoPreference);
  } else if (name_ == preference_names::kReducedData) {
    std::optional<bool> reduced_data = overrides->GetPrefersReducedData();
    if (!reduced_data.has_value()) {
      return std::nullopt;
    }

    return std::make_optional(reduced_data ? preference_values::kReduce
                                           : preference_values::kNoPreference);
  } else {
    NOTREACHED_IN_MIGRATION();
    return std::nullopt;
  }
}

AtomicString PreferenceObject::value(ScriptState* script_state) {
  CHECK(RuntimeEnabledFeatures::WebPreferencesEnabled());
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

  if (name_ == preference_names::kColorScheme) {
    return ColorSchemeToString(preferred_color_scheme_);
  } else if (name_ == preference_names::kContrast) {
    return ContrastToString(preferred_contrast_);
  } else if (name_ == preference_names::kReducedMotion) {
    return prefers_reduced_motion_ ? preference_values::kReduce
                                   : preference_values::kNoPreference;
  } else if (name_ == preference_names::kReducedTransparency) {
    return prefers_reduced_transparency_ ? preference_values::kReduce
                                         : preference_values::kNoPreference;
  } else if (name_ == preference_names::kReducedData) {
    return prefers_reduced_data_ ? preference_values::kReduce
                                 : preference_values::kNoPreference;
  } else {
    NOTREACHED_IN_MIGRATION();
    return g_empty_atom;
  }
}

void PreferenceObject::clearOverride(ScriptState* script_state) {
  CHECK(RuntimeEnabledFeatures::WebPreferencesEnabled());
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

  bool value_unchanged;
  if (name_ == preference_names::kColorScheme) {
    std::optional<mojom::blink::PreferredColorScheme> color_scheme =
        overrides->GetPreferredColorScheme();
    if (!color_scheme.has_value()) {
      return;
    }

    window->GetFrame()->GetPage()->SetPreferenceOverride(
        media_feature_names::kPrefersColorSchemeMediaFeature, String());
    value_unchanged =
        (color_scheme.value() == media_values_->GetPreferredColorScheme());
  } else if (name_ == preference_names::kContrast) {
    std::optional<mojom::blink::PreferredContrast> contrast =
        overrides->GetPreferredContrast();
    if (!contrast.has_value()) {
      return;
    }

    window->GetFrame()->GetPage()->SetPreferenceOverride(
        media_feature_names::kPrefersContrastMediaFeature, String());
    value_unchanged =
        (contrast.value() == media_values_->GetPreferredContrast());
  } else if (name_ == preference_names::kReducedMotion) {
    std::optional<bool> reduced_motion = overrides->GetPrefersReducedMotion();
    if (!reduced_motion.has_value()) {
      return;
    }

    window->GetFrame()->GetPage()->SetPreferenceOverride(
        media_feature_names::kPrefersReducedMotionMediaFeature, String());
    value_unchanged =
        (reduced_motion.value() == media_values_->PrefersReducedMotion());
  } else if (name_ == preference_names::kReducedTransparency) {
    std::optional<bool> reduced_transparency =
        overrides->GetPrefersReducedTransparency();
    if (!reduced_transparency.has_value()) {
      return;
    }

    window->GetFrame()->GetPage()->SetPreferenceOverride(
        media_feature_names::kPrefersReducedTransparencyMediaFeature, String());
    value_unchanged = (reduced_transparency.value() ==
                       media_values_->PrefersReducedTransparency());
  } else if (name_ == preference_names::kReducedData) {
    std::optional<bool> reduced_data = overrides->GetPrefersReducedData();
    if (!reduced_data.has_value()) {
      return;
    }

    window->GetFrame()->GetPage()->SetPreferenceOverride(
        media_feature_names::kPrefersReducedDataMediaFeature, String());
    value_unchanged =
        (reduced_data.value() == media_values_->PrefersReducedData());
  } else {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  if (value_unchanged) {
    DispatchEvent(*Event::Create(event_type_names::kChange));
  }
}

ScriptPromise<IDLUndefined> PreferenceObject::requestOverride(
    ScriptState* script_state,
    std::optional<AtomicString> value) {
  CHECK(RuntimeEnabledFeatures::WebPreferencesEnabled());
  if (!script_state || !script_state->ContextIsValid()) {
    return EmptyPromise();
  }
  auto* execution_context = ExecutionContext::From(script_state);
  if (!execution_context || execution_context->IsContextDestroyed()) {
    return EmptyPromise();
  }
  auto* window = DynamicTo<LocalDOMWindow>(execution_context);
  if (!window) {
    return EmptyPromise();
  }

  if (!value.has_value() || value.value().empty()) {
    clearOverride(script_state);
    return ToResolvedUndefinedPromise(script_state);
  }

  AtomicString feature_name;
  AtomicString new_value;
  bool has_existing_override = false;
  bool value_same_as_existing_override = false;

  AtomicString existing_value;

  if (validValues().AsVector().Contains(value.value())) {
    new_value = value.value();
  }

  const PreferenceOverrides* overrides =
      window->GetFrame()->GetPage()->GetPreferenceOverrides();
  if (name_ == preference_names::kColorScheme) {
    feature_name = media_feature_names::kPrefersColorSchemeMediaFeature;

    if (overrides) {
      auto override = overrides->GetPreferredColorScheme();
      if (override.has_value()) {
        has_existing_override = true;
        if (new_value == ColorSchemeToString(override.value()).GetString()) {
          value_same_as_existing_override = true;
        }
      }
    }
    existing_value = ColorSchemeToString(preferred_color_scheme_);
  } else if (name_ == preference_names::kContrast) {
    feature_name = media_feature_names::kPrefersContrastMediaFeature;

    if (overrides) {
      auto override = overrides->GetPreferredContrast();
      if (override.has_value()) {
        has_existing_override = true;
        if (new_value == ContrastToString(override.value()).GetString()) {
          value_same_as_existing_override = true;
        }
      }
    }
    existing_value = ContrastToString(preferred_contrast_);
  } else if (name_ == preference_names::kReducedMotion) {
    feature_name = media_feature_names::kPrefersReducedMotionMediaFeature;

    if (overrides) {
      auto override = overrides->GetPrefersReducedMotion();
      if (override.has_value()) {
        has_existing_override = true;
        if ((new_value == preference_values::kReduce && override.value()) ||
            (new_value == preference_values::kNoPreference &&
             !override.value())) {
          value_same_as_existing_override = true;
        }
      }
    }
    existing_value = prefers_reduced_motion_ ? preference_values::kReduce
                                             : preference_values::kNoPreference;
  } else if (name_ == preference_names::kReducedTransparency) {
    feature_name = media_feature_names::kPrefersReducedTransparencyMediaFeature;

    if (overrides) {
      auto override = overrides->GetPrefersReducedTransparency();
      if (override.has_value()) {
        has_existing_override = true;
        if ((new_value == preference_values::kReduce && override.value()) ||
            (new_value == preference_values::kNoPreference &&
             !override.value())) {
          value_same_as_existing_override = true;
        }
      }
    }
    existing_value = prefers_reduced_transparency_
                         ? preference_values::kReduce
                         : preference_values::kNoPreference;
  } else if (name_ == preference_names::kReducedData) {
    feature_name = media_feature_names::kPrefersReducedDataMediaFeature;

    if (overrides) {
      auto override = overrides->GetPrefersReducedData();
      if (override.has_value()) {
        has_existing_override = true;
        if ((new_value == preference_values::kReduce && override.value()) ||
            (new_value == preference_values::kNoPreference &&
             !override.value())) {
          value_same_as_existing_override = true;
        }
      }
    }
    existing_value = prefers_reduced_data_ ? preference_values::kReduce
                                           : preference_values::kNoPreference;
  } else {
    NOTREACHED_IN_MIGRATION();
  }

  if (new_value.empty()) {
    return ScriptPromise<IDLUndefined>::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kTypeMismatchError,
                          value.value() + " is not a valid value."));
  }

  if (!value_same_as_existing_override) {
    window->GetFrame()->GetPage()->SetPreferenceOverride(feature_name,
                                                         new_value.GetString());
  }

  if (!has_existing_override && new_value == existing_value) {
    DispatchEvent(*Event::Create(event_type_names::kChange));
  }

  return ToResolvedUndefinedPromise(script_state);
}

const FrozenArray<IDLString>& PreferenceObject::validValues() {
  CHECK(RuntimeEnabledFeatures::WebPreferencesEnabled());
  if (valid_values_) [[likely]] {
    return *valid_values_.Get();
  }

  FrozenArray<IDLString>::VectorType valid_values;
  if (name_ == preference_names::kColorScheme) {
    valid_values.push_back(preference_values::kLight);
    valid_values.push_back(preference_values::kDark);
  } else if (name_ == preference_names::kContrast) {
    valid_values.push_back(preference_values::kMore);
    valid_values.push_back(preference_values::kLess);
    valid_values.push_back(preference_values::kNoPreference);
  } else if (name_ == preference_names::kReducedMotion) {
    valid_values.push_back(preference_values::kReduce);
    valid_values.push_back(preference_values::kNoPreference);
  } else if (name_ == preference_names::kReducedTransparency) {
    valid_values.push_back(preference_values::kReduce);
    valid_values.push_back(preference_values::kNoPreference);
  } else if (name_ == preference_names::kReducedData) {
    valid_values.push_back(preference_values::kReduce);
    valid_values.push_back(preference_values::kNoPreference);
  } else {
    NOTREACHED_IN_MIGRATION();
  }
  valid_values_ =
      MakeGarbageCollected<FrozenArray<IDLString>>(std::move(valid_values));
  return *valid_values_.Get();
}

void PreferenceObject::PreferenceMaybeChanged() {
  CHECK(RuntimeEnabledFeatures::WebPreferencesEnabled());
  if (!GetExecutionContext() || GetExecutionContext()->IsContextDestroyed()) {
    return;
  }
  auto* window = DynamicTo<LocalDOMWindow>(GetExecutionContext());
  if (!window) {
    return;
  }

  if (name_ == preference_names::kColorScheme) {
    if (preferred_color_scheme_ == media_values_->GetPreferredColorScheme()) {
      return;
    }
  } else if (name_ == preference_names::kContrast) {
    if (preferred_contrast_ == media_values_->GetPreferredContrast()) {
      return;
    }
  } else if (name_ == preference_names::kReducedMotion) {
    if (prefers_reduced_motion_ == media_values_->PrefersReducedMotion()) {
      return;
    }
  } else if (name_ == preference_names::kReducedTransparency) {
    if (prefers_reduced_transparency_ ==
        media_values_->PrefersReducedTransparency()) {
      return;
    }
  } else if (name_ == preference_names::kReducedData) {
    if (prefers_reduced_data_ == media_values_->PrefersReducedData()) {
      return;
    }
  } else {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  preferred_color_scheme_ = media_values_->GetPreferredColorScheme();
  preferred_contrast_ = media_values_->GetPreferredContrast();
  prefers_reduced_data_ = media_values_->PrefersReducedData();
  prefers_reduced_motion_ = media_values_->PrefersReducedMotion();
  prefers_reduced_transparency_ = media_values_->PrefersReducedTransparency();
  DispatchEvent(*Event::Create(event_type_names::kChange));
}

void PreferenceObject::Trace(Visitor* visitor) const {
  EventTarget::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  visitor->Trace(valid_values_);
  visitor->Trace(media_values_);
}

const AtomicString& PreferenceObject::InterfaceName() const {
  return event_target_names::kPreferenceObject;
}

void PreferenceObject::ContextDestroyed() {
  RemoveAllEventListeners();
}

ExecutionContext* PreferenceObject::GetExecutionContext() const {
  return ExecutionContextLifecycleObserver::GetExecutionContext();
}

}  // namespace blink
