// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_paint_value.h"

#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_paint_image_generator.h"
#include "third_party/blink/renderer/core/css/css_syntax_definition.h"
#include "third_party/blink/renderer/core/css/cssom/css_paint_worklet_input.h"
#include "third_party/blink/renderer/core/css/cssom/paint_worklet_deferred_image.h"
#include "third_party/blink/renderer/core/css/cssom/style_value_factory.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/platform_paint_worklet_layer_painter.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSPaintValue::CSSPaintValue(CSSCustomIdentValue* name,
                             bool threaded_compositing_enabled)
    : CSSImageGeneratorValue(kPaintClass),
      name_(name),
      paint_image_generator_observer_(MakeGarbageCollected<Observer>(this)),
      off_thread_paint_state_(
          (!threaded_compositing_enabled ||
           !RuntimeEnabledFeatures::OffMainThreadCSSPaintEnabled())
              ? OffThreadPaintState::kMainThread
              : OffThreadPaintState::kUnknown) {}

CSSPaintValue::CSSPaintValue(CSSCustomIdentValue* name)
    : CSSPaintValue(name, Thread::CompositorThread()) {}

CSSPaintValue::CSSPaintValue(
    CSSCustomIdentValue* name,
    HeapVector<Member<CSSVariableData>>&& variable_data)
    : CSSPaintValue(name) {
  argument_variable_data_ = variable_data;
}

CSSPaintValue::~CSSPaintValue() = default;

String CSSPaintValue::CustomCSSText() const {
  StringBuilder result;
  result.Append("paint(");
  result.Append(name_->CustomCSSText());
  for (const auto& variable_data : argument_variable_data_) {
    result.Append(", ");
    result.Append(variable_data.Get()->Serialize());
  }
  result.Append(')');
  return result.ReleaseString();
}

String CSSPaintValue::GetName() const {
  return name_->Value();
}

const Vector<CSSPropertyID>* CSSPaintValue::NativeInvalidationProperties(
    const Document& document) const {
  auto it = generators_.find(&document);
  if (it == generators_.end()) {
    return nullptr;
  }
  return &it->value->NativeInvalidationProperties();
}

const Vector<AtomicString>* CSSPaintValue::CustomInvalidationProperties(
    const Document& document) const {
  auto it = generators_.find(&document);
  if (it == generators_.end()) {
    return nullptr;
  }
  return &it->value->CustomInvalidationProperties();
}

bool CSSPaintValue::IsUsingCustomProperty(
    const AtomicString& custom_property_name,
    const Document& document) const {
  auto it = generators_.find(&document);
  if (it == generators_.end() || !it->value->IsImageGeneratorReady()) {
    return false;
  }
  return it->value->CustomInvalidationProperties().Contains(
      custom_property_name);
}

CSSPaintImageGenerator& CSSPaintValue::EnsureGenerator(
    const Document& document) {
  auto& generator = generators_.insert(&document, nullptr).stored_value->value;
  if (!generator) {
    generator = CSSPaintImageGenerator::Create(GetName(), document,
                                               paint_image_generator_observer_);
  }
  return *generator;
}

scoped_refptr<Image> CSSPaintValue::GetImage(
    const ImageResourceObserver& client,
    const Document& document,
    const ComputedStyle& style,
    const gfx::SizeF& target_size) {
  // https://crbug.com/835589: early exit when paint target is associated with
  // a link.
  if (style.InsideLink() != EInsideLink::kNotInsideLink) {
    return nullptr;
  }

  CSSPaintImageGenerator& generator = EnsureGenerator(document);

  // If the generator isn't ready yet, we have nothing to paint. Our
  // |paint_image_generator_observer_| will cause us to be called again once the
  // generator is ready.
  if (!generator.IsImageGeneratorReady()) {
    return nullptr;
  }

  if (!ParseInputArguments(document)) {
    return nullptr;
  }

  // TODO(crbug.com/946515): Break dependency on LayoutObject.
  const LayoutObject& layout_object = static_cast<const LayoutObject&>(client);

  // For Off-Thread PaintWorklet, we just collect the necessary inputs together
  // and defer the actual JavaScript call until much later (during cc Raster).
  //
  // Generating print-previews happens entirely on the main thread, so we have
  // to fall-back to main in that case.
  if (off_thread_paint_state_ != OffThreadPaintState::kMainThread &&
      !document.Printing()) {
    // It is not necessary for a LayoutObject to always have RareData which
    // contains the ElementId. If this |layout_object| doesn't have an
    // ElementId, then create one for it.
    layout_object.GetMutableForPainting().EnsureId();

    const Vector<CSSPropertyID>& native_properties =
        generator.NativeInvalidationProperties();
    const Vector<AtomicString>& custom_properties =
        generator.CustomInvalidationProperties();
    float zoom = layout_object.StyleRef().EffectiveZoom();
    CompositorPaintWorkletInput::PropertyKeys input_property_keys;
    auto style_data = PaintWorkletStylePropertyMap::BuildCrossThreadData(
        document, layout_object.UniqueId(), style, native_properties,
        custom_properties, input_property_keys);
    off_thread_paint_state_ = style_data.has_value()
                                  ? OffThreadPaintState::kOffThread
                                  : OffThreadPaintState::kMainThread;
    if (off_thread_paint_state_ == OffThreadPaintState::kOffThread) {
      Vector<std::unique_ptr<CrossThreadStyleValue>>
          cross_thread_input_arguments;
      BuildInputArgumentValues(cross_thread_input_arguments);
      scoped_refptr<CSSPaintWorkletInput> input =
          base::MakeRefCounted<CSSPaintWorkletInput>(
              GetName(), target_size, zoom, generator.WorkletId(),
              std::move(style_data.value()),
              std::move(cross_thread_input_arguments),
              std::move(input_property_keys));
      return PaintWorkletDeferredImage::Create(std::move(input), target_size);
    }
  }

  return generator.Paint(client, target_size, parsed_input_arguments_.Get());
}

void CSSPaintValue::BuildInputArgumentValues(
    Vector<std::unique_ptr<CrossThreadStyleValue>>&
        cross_thread_input_arguments) {
  if (!parsed_input_arguments_) {
    return;
  }
  for (const auto& style_value : *parsed_input_arguments_) {
    std::unique_ptr<CrossThreadStyleValue> cross_thread_style =
        ComputedStyleUtils::CrossThreadStyleValueFromCSSStyleValue(style_value);
    cross_thread_input_arguments.push_back(std::move(cross_thread_style));
  }
}

bool CSSPaintValue::ParseInputArguments(const Document& document) {
  if (input_arguments_invalid_) {
    return false;
  }

  if (parsed_input_arguments_ ||
      !RuntimeEnabledFeatures::CSSPaintAPIArgumentsEnabled()) {
    return true;
  }

  auto it = generators_.find(&document);
  if (it == generators_.end()) {
    input_arguments_invalid_ = true;
    return false;
  }
  DCHECK(it->value->IsImageGeneratorReady());
  const Vector<CSSSyntaxDefinition>& input_argument_types =
      it->value->InputArgumentTypes();
  if (argument_variable_data_.size() != input_argument_types.size()) {
    input_arguments_invalid_ = true;
    return false;
  }

  parsed_input_arguments_ = MakeGarbageCollected<CSSStyleValueVector>();

  for (wtf_size_t i = 0; i < argument_variable_data_.size(); ++i) {
    // If we are parsing a paint() function, we must be a secure context.
    DCHECK_EQ(SecureContextMode::kSecureContext,
              document.GetExecutionContext()->GetSecureContextMode());
    DCHECK(!argument_variable_data_[i]->NeedsVariableResolution());
    const CSSValue* parsed_value = argument_variable_data_[i]->ParseForSyntax(
        input_argument_types[i], SecureContextMode::kSecureContext);
    if (!parsed_value) {
      input_arguments_invalid_ = true;
      parsed_input_arguments_ = nullptr;
      return false;
    }
    parsed_input_arguments_->AppendVector(
        StyleValueFactory::CssValueToStyleValueVector(*parsed_value));
  }
  return true;
}

void CSSPaintValue::Observer::PaintImageGeneratorReady() {
  owner_value_->PaintImageGeneratorReady();
}

void CSSPaintValue::PaintImageGeneratorReady() {
  for (const ImageResourceObserver* client : Clients().Keys()) {
    // TODO(ikilpatrick): We shouldn't be casting like this or mutate the layout
    // tree from a const pointer.
    const_cast<ImageResourceObserver*>(client)->ImageChanged(
        static_cast<WrappedImagePtr>(this),
        ImageResourceObserver::CanDeferInvalidation::kNo);
  }
}

bool CSSPaintValue::KnownToBeOpaque(const Document& document,
                                    const ComputedStyle&) const {
  auto it = generators_.find(&document);
  return it != generators_.end() && !it->value->HasAlpha();
}

bool CSSPaintValue::Equals(const CSSPaintValue& other) const {
  return GetName() == other.GetName() &&
         CustomCSSText() == other.CustomCSSText();
}

void CSSPaintValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(name_);
  visitor->Trace(generators_);
  visitor->Trace(paint_image_generator_observer_);
  visitor->Trace(parsed_input_arguments_);
  visitor->Trace(argument_variable_data_);
  CSSImageGeneratorValue::TraceAfterDispatch(visitor);
}

}  // namespace blink
