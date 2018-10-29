// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_CSS_PAINT_DEFINITION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_CSS_PAINT_DEFINITION_H_

#include "third_party/blink/renderer/core/css/css_syntax_descriptor.h"
#include "third_party/blink/renderer/core/css/cssom/css_style_value.h"
#include "third_party/blink/renderer/core/css_property_names.h"
#include "third_party/blink/renderer/modules/csspaint/paint_rendering_context_2d_settings.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "v8/include/v8.h"

namespace blink {

class Image;
class ScriptState;
class ImageResourceObserver;

// Represents a javascript class registered on the PaintWorkletGlobalScope by
// the author. It will store the properties for invalidation and input argument
// types as well.
class MODULES_EXPORT CSSPaintDefinition final
    : public GarbageCollectedFinalized<CSSPaintDefinition>,
      public NameClient {
 public:
  static CSSPaintDefinition* Create(
      ScriptState*,
      v8::Local<v8::Function> constructor,
      v8::Local<v8::Function> paint,
      const Vector<CSSPropertyID>&,
      const Vector<AtomicString>& custom_invalidation_properties,
      const Vector<CSSSyntaxDescriptor>& input_argument_types,
      const PaintRenderingContext2DSettings&);
  virtual ~CSSPaintDefinition();

  // Invokes the javascript 'paint' callback on an instance of the javascript
  // class. The size given will be the size of the PaintRenderingContext2D
  // given to the callback.
  //
  // This may return a nullptr (representing an invalid image) if javascript
  // throws an error.
  //
  // The |container_size| is without subpixel snapping.
  scoped_refptr<Image> Paint(const ImageResourceObserver&,
                             const FloatSize& container_size,
                             const CSSStyleValueVector*);
  const Vector<CSSPropertyID>& NativeInvalidationProperties() const {
    return native_invalidation_properties_;
  }
  const Vector<AtomicString>& CustomInvalidationProperties() const {
    return custom_invalidation_properties_;
  }
  const Vector<CSSSyntaxDescriptor>& InputArgumentTypes() const {
    return input_argument_types_;
  }
  const PaintRenderingContext2DSettings& GetPaintRenderingContext2DSettings()
      const {
    return context_settings_;
  }

  ScriptState* GetScriptState() const { return script_state_; }

  v8::Local<v8::Function> PaintFunctionForTesting(v8::Isolate* isolate) {
    return paint_.NewLocal(isolate);
  }

  virtual void Trace(blink::Visitor* visitor);
  const char* NameInHeapSnapshot() const override {
    return "CSSPaintDefinition";
  }

 private:
  CSSPaintDefinition(
      ScriptState*,
      v8::Local<v8::Function> constructor,
      v8::Local<v8::Function> paint,
      const Vector<CSSPropertyID>& native_invalidation_properties,
      const Vector<AtomicString>& custom_invalidation_properties,
      const Vector<CSSSyntaxDescriptor>& input_argument_types,
      const PaintRenderingContext2DSettings&);

  void MaybeCreatePaintInstance();

  Member<ScriptState> script_state_;

  // This object keeps the class instance object, constructor function and
  // paint function alive. It participates in wrapper tracing as it holds onto
  // V8 wrappers.
  TraceWrapperV8Reference<v8::Function> constructor_;
  TraceWrapperV8Reference<v8::Function> paint_;

  // At the moment there is only ever one instance of a paint class per type.
  TraceWrapperV8Reference<v8::Value> instance_;

  bool did_call_constructor_;

  Vector<CSSPropertyID> native_invalidation_properties_;
  Vector<AtomicString> custom_invalidation_properties_;
  // Input argument types, if applicable.
  Vector<CSSSyntaxDescriptor> input_argument_types_;
  PaintRenderingContext2DSettings context_settings_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CSSPAINT_CSS_PAINT_DEFINITION_H_
