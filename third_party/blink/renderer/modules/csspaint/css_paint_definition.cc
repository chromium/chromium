// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/css_paint_definition.h"

#include <memory>

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_no_argument_constructor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_paint_callback.h"
#include "third_party/blink/renderer/core/css/css_computed_style_declaration.h"
#include "third_party/blink/renderer/core/css/cssom/cross_thread_color_value.h"
#include "third_party/blink/renderer/core/css/cssom/cross_thread_unit_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_paint_worklet_input.h"
#include "third_party/blink/renderer/core/css/cssom/prepopulated_computed_style_property_map.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/csspaint/paint_rendering_context_2d.h"
#include "third_party/blink/renderer/modules/csspaint/paint_size.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding_macros.h"
#include "third_party/blink/renderer/platform/graphics/paint_generated_image.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

namespace {

gfx::SizeF GetSpecifiedSize(const gfx::SizeF& size, float zoom) {
  float un_zoom_factor = 1 / zoom;
  auto un_zoom_fn = [un_zoom_factor](float a) -> float {
    return a * un_zoom_factor;
  };
  return gfx::SizeF(un_zoom_fn(size.width()), un_zoom_fn(size.height()));
}

}  // namespace

CSSPaintDefinition::CSSPaintDefinition(
    ScriptState* script_state,
    V8NoArgumentConstructor* constructor,
    V8PaintCallback* paint,
    const Vector<CSSPropertyID>& native_invalidation_properties,
    const Vector<AtomicString>& custom_invalidation_properties,
    const Vector<CSSSyntaxDefinition>& input_argument_types,
    const PaintRenderingContext2DSettings* context_settings,
    PaintWorkletGlobalScope* global_scope)
    : script_state_(script_state),
      constructor_(constructor),
      paint_(paint),
      did_call_constructor_(false),
      context_settings_(context_settings),
      global_scope_(global_scope) {
  native_invalidation_properties_ = native_invalidation_properties;
  custom_invalidation_properties_ = custom_invalidation_properties;
  input_argument_types_ = input_argument_types;
}

CSSPaintDefinition::~CSSPaintDefinition() = default;

// PaintDefinition override
PaintRecord CSSPaintDefinition::Paint(
    const CompositorPaintWorkletInput* compositor_input,
    const CompositorPaintWorkletJob::AnimatedPropertyValues&
        animated_property_values) {
  const CSSPaintWorkletInput* input =
      To<CSSPaintWorkletInput>(compositor_input);
  PaintWorkletStylePropertyMap* style_map =
      MakeGarbageCollected<PaintWorkletStylePropertyMap>(input->StyleMapData());
  CSSStyleValueVector paint_arguments;
  for (const auto& style_value : input->ParsedInputArguments()) {
    paint_arguments.push_back(style_value->ToCSSStyleValue());
  }

  ApplyAnimatedPropertyOverrides(style_map, animated_property_values);

  return Paint(input->GetSize(), input->EffectiveZoom(), style_map,
               &paint_arguments);
}

PaintRecord CSSPaintDefinition::Paint(
    const gfx::SizeF& container_size,
    float zoom,
    StylePropertyMapReadOnly* style_map,
    const CSSStyleValueVector* paint_arguments) {
  const gfx::SizeF specified_size = GetSpecifiedSize(container_size, zoom);
  ScriptState::Scope scope(script_state_);

  MaybeCreatePaintInstance();
  // We may have failed to create an instance, in which case produce an
  // invalid image.
  if (instance_.IsEmpty())
    return PaintRecord();

  v8::Isolate* isolate = script_state_->GetIsolate();

  // Do subpixel snapping for the |container_size|.
  auto* rendering_context = MakeGarbageCollected<PaintRenderingContext2D>(
      ToRoundedSize(container_size), context_settings_, zoom,
      global_scope_->GetTaskRunner(TaskType::kMiscPlatformAPI), global_scope_);
  PaintSize* paint_size = MakeGarbageCollected<PaintSize>(specified_size);

  CSSStyleValueVector empty_paint_arguments;
  if (!paint_arguments)
    paint_arguments = &empty_paint_arguments;

  v8::TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);

  // The paint function may have produced an error, in which case produce an
  // invalid image.
  if (paint_
          ->Invoke(instance_.Get(isolate), rendering_context, paint_size,
                   style_map, *paint_arguments)
          .IsNothing()) {
    return PaintRecord();
  }

  return rendering_context->GetRecord();
}

void CSSPaintDefinition::ApplyAnimatedPropertyOverrides(
    PaintWorkletStylePropertyMap* style_map,
    const CompositorPaintWorkletJob::AnimatedPropertyValues&
        animated_property_values) {
  for (const auto& property_value : animated_property_values) {
    DCHECK(property_value.second.has_value());
    String property_name(
        property_value.first.custom_property_name.value().c_str());
    DCHECK(style_map->StyleMapData().Contains(property_name));
    CrossThreadStyleValue* old_value =
        style_map->StyleMapData().at(property_name);
    switch (old_value->GetType()) {
      case CrossThreadStyleValue::StyleValueType::kUnitType: {
        DCHECK(property_value.second.float_value);
        std::unique_ptr<CrossThreadUnitValue> new_value =
            std::make_unique<CrossThreadUnitValue>(
                property_value.second.float_value.value(),
                DynamicTo<CrossThreadUnitValue>(old_value)->GetUnitType());
        style_map->StyleMapData().Set(property_name, std::move(new_value));
        break;
      }
      case CrossThreadStyleValue::StyleValueType::kColorType: {
        DCHECK(property_value.second.color_value);
        std::unique_ptr<CrossThreadColorValue> new_value =
            std::make_unique<CrossThreadColorValue>(Color::FromSkColor4f(
                property_value.second.color_value.value()));
        style_map->StyleMapData().Set(property_name, std::move(new_value));
        break;
      }
      default:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }
}

void CSSPaintDefinition::MaybeCreatePaintInstance() {
  if (did_call_constructor_)
    return;
  did_call_constructor_ = true;

  DCHECK(instance_.IsEmpty());

  ScriptValue paint_instance;
  if (!constructor_->Construct().To(&paint_instance))
    return;

  instance_.Reset(constructor_->GetIsolate(), paint_instance.V8Value());
}

void CSSPaintDefinition::Trace(Visitor* visitor) const {
  visitor->Trace(constructor_);
  visitor->Trace(paint_);
  visitor->Trace(instance_);
  visitor->Trace(context_settings_);
  visitor->Trace(script_state_);
  visitor->Trace(global_scope_);
  PaintDefinition::Trace(visitor);
}

}  // namespace blink
