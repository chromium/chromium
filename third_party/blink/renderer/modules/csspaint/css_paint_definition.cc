// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/css_paint_definition.h"

#include <memory>
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/css/css_computed_style_declaration.h"
#include "third_party/blink/renderer/core/css/cssom/prepopulated_computed_style_property_map.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/modules/csspaint/paint_rendering_context_2d.h"
#include "third_party/blink/renderer/modules/csspaint/paint_size.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding_macros.h"
#include "third_party/blink/renderer/platform/bindings/v8_object_constructor.h"
#include "third_party/blink/renderer/platform/graphics/paint_generated_image.h"

namespace blink {

namespace {

FloatSize GetSpecifiedSize(const FloatSize& size, float zoom) {
  float un_zoom_factor = 1 / zoom;
  auto un_zoom_fn = [un_zoom_factor](float a) -> float {
    return a * un_zoom_factor;
  };
  return FloatSize(un_zoom_fn(size.Width()), un_zoom_fn(size.Height()));
}

}  // namespace

CSSPaintDefinition* CSSPaintDefinition::Create(
    ScriptState* script_state,
    v8::Local<v8::Function> constructor,
    v8::Local<v8::Function> paint,
    const Vector<CSSPropertyID>& native_invalidation_properties,
    const Vector<AtomicString>& custom_invalidation_properties,
    const Vector<CSSSyntaxDescriptor>& input_argument_types,
    const PaintRenderingContext2DSettings& context_settings) {
  return new CSSPaintDefinition(
      script_state, constructor, paint, native_invalidation_properties,
      custom_invalidation_properties, input_argument_types, context_settings);
}

CSSPaintDefinition::CSSPaintDefinition(
    ScriptState* script_state,
    v8::Local<v8::Function> constructor,
    v8::Local<v8::Function> paint,
    const Vector<CSSPropertyID>& native_invalidation_properties,
    const Vector<AtomicString>& custom_invalidation_properties,
    const Vector<CSSSyntaxDescriptor>& input_argument_types,
    const PaintRenderingContext2DSettings& context_settings)
    : script_state_(script_state),
      constructor_(script_state->GetIsolate(), constructor),
      paint_(script_state->GetIsolate(), paint),
      did_call_constructor_(false),
      context_settings_(context_settings) {
  native_invalidation_properties_ = native_invalidation_properties;
  custom_invalidation_properties_ = custom_invalidation_properties;
  input_argument_types_ = input_argument_types;
}

CSSPaintDefinition::~CSSPaintDefinition() = default;

scoped_refptr<Image> CSSPaintDefinition::Paint(
    const ImageResourceObserver& client,
    const FloatSize& container_size,
    const CSSStyleValueVector* paint_arguments) {
  // TODO: Break dependency on LayoutObject. Passing the Node should work.
  const LayoutObject& layout_object = static_cast<const LayoutObject&>(client);

  float zoom = layout_object.StyleRef().EffectiveZoom();
  const FloatSize specified_size = GetSpecifiedSize(container_size, zoom);

  ScriptState::Scope scope(script_state_);

  MaybeCreatePaintInstance();

  v8::Isolate* isolate = script_state_->GetIsolate();
  v8::Local<v8::Value> instance = instance_.NewLocal(isolate);

  // We may have failed to create an instance class, in which case produce an
  // invalid image.
  if (IsUndefinedOrNull(instance))
    return nullptr;

  DCHECK(layout_object.GetNode());
  CanvasColorParams color_params;
  if (!context_settings_.alpha()) {
    color_params.SetOpacityMode(kOpaque);
  }

  // Do subpixel snapping for the |container_size|.
  PaintRenderingContext2D* rendering_context = PaintRenderingContext2D::Create(
      RoundedIntSize(container_size), color_params, context_settings_, zoom);
  PaintSize* paint_size = PaintSize::Create(specified_size);
  StylePropertyMapReadOnly* style_map =
      new PrepopulatedComputedStylePropertyMap(
          layout_object.GetDocument(), layout_object.StyleRef(),
          layout_object.GetNode(), native_invalidation_properties_,
          custom_invalidation_properties_);

  Vector<v8::Local<v8::Value>, 4> argv;
  if (paint_arguments) {
    argv = {
        ToV8(rendering_context, script_state_->GetContext()->Global(), isolate),
        ToV8(paint_size, script_state_->GetContext()->Global(), isolate),
        ToV8(style_map, script_state_->GetContext()->Global(), isolate),
        ToV8(*paint_arguments, script_state_->GetContext()->Global(), isolate)};
  } else {
    argv = {
        ToV8(rendering_context, script_state_->GetContext()->Global(), isolate),
        ToV8(paint_size, script_state_->GetContext()->Global(), isolate),
        ToV8(style_map, script_state_->GetContext()->Global(), isolate)};
  }

  v8::Local<v8::Function> paint = paint_.NewLocal(isolate);

  v8::TryCatch block(isolate);
  block.SetVerbose(true);

  V8ScriptRunner::CallFunction(paint, ExecutionContext::From(script_state_),
                               instance, argv.size(), argv.data(), isolate);

  // The paint function may have produced an error, in which case produce an
  // invalid image.
  if (block.HasCaught()) {
    return nullptr;
  }

  return PaintGeneratedImage::Create(rendering_context->GetRecord(),
                                     container_size);
}

void CSSPaintDefinition::MaybeCreatePaintInstance() {
  if (did_call_constructor_)
    return;

  DCHECK(instance_.IsEmpty());

  v8::Isolate* isolate = script_state_->GetIsolate();
  v8::Local<v8::Function> constructor = constructor_.NewLocal(isolate);
  DCHECK(!IsUndefinedOrNull(constructor));

  v8::Local<v8::Value> paint_instance;
  if (V8ScriptRunner::CallAsConstructor(
          isolate, constructor, ExecutionContext::From(script_state_), 0, {})
          .ToLocal(&paint_instance))
    instance_.Set(isolate, paint_instance);

  did_call_constructor_ = true;
}

void CSSPaintDefinition::Trace(Visitor* visitor) {
  visitor->Trace(constructor_.Cast<v8::Value>());
  visitor->Trace(paint_.Cast<v8::Value>());
  visitor->Trace(instance_);
  visitor->Trace(script_state_);
}

}  // namespace blink
