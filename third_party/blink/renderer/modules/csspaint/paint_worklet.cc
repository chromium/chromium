// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/paint_worklet.h"

#include "base/atomic_sequence_num.h"
#include "base/rand_util.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/css/cssom/prepopulated_computed_style_property_map.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node_rare_data.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/csspaint/css_paint_definition.h"
#include "third_party/blink/renderer/modules/csspaint/paint_worklet_global_scope.h"
#include "third_party/blink/renderer/modules/csspaint/paint_worklet_messaging_proxy.h"
#include "third_party/blink/renderer/platform/graphics/paint_generated_image.h"

namespace blink {

namespace {
base::AtomicSequenceNumber g_next_worklet_id;
int NextId() {
  // Start id from 1. This way it safe to use it as key in hashmap with default
  // key traits.
  return g_next_worklet_id.GetNext() + 1;
}
}  // namespace

const wtf_size_t PaintWorklet::kNumGlobalScopesPerThread = 2u;
const size_t kMaxPaintCountToSwitch = 30u;

// static
PaintWorklet* PaintWorklet::From(LocalDOMWindow& window) {
  PaintWorklet* supplement =
      Supplement<LocalDOMWindow>::From<PaintWorklet>(window);
  if (!supplement && window.GetFrame()) {
    supplement = MakeGarbageCollected<PaintWorklet>(window.GetFrame());
    ProvideTo(window, supplement);
  }
  return supplement;
}

PaintWorklet::PaintWorklet(LocalFrame* frame)
    : Worklet(frame->GetDocument()),
      Supplement<LocalDOMWindow>(*frame->DomWindow()),
      pending_generator_registry_(
          MakeGarbageCollected<PaintWorkletPendingGeneratorRegistry>()),
      worklet_id_(NextId()) {}

PaintWorklet::~PaintWorklet() = default;

void PaintWorklet::AddPendingGenerator(const String& name,
                                       CSSPaintImageGeneratorImpl* generator) {
  pending_generator_registry_->AddPendingGenerator(name, generator);
}

// We start with a random global scope when a new frame starts. Then within this
// frame, we switch to the other global scope after certain amount of paint
// calls (rand(kMaxPaintCountToSwitch)).
// This approach ensures non-deterministic of global scope selecting, and that
// there is a max of one switching within one frame.
wtf_size_t PaintWorklet::SelectGlobalScope() {
  size_t current_paint_frame_count = GetFrame()->View()->PaintFrameCount();
  // Whether a new frame starts or not.
  bool frame_changed = current_paint_frame_count != active_frame_count_;
  if (frame_changed) {
    paints_before_switching_global_scope_ = GetPaintsBeforeSwitching();
    active_frame_count_ = current_paint_frame_count;
  }
  // We switch when |paints_before_switching_global_scope_| is 1 instead of 0
  // because the var keeps decrementing and stays at 0.
  if (frame_changed || paints_before_switching_global_scope_ == 1)
    active_global_scope_ = SelectNewGlobalScope();
  if (paints_before_switching_global_scope_ > 0)
    paints_before_switching_global_scope_--;
  return active_global_scope_;
}

int PaintWorklet::GetPaintsBeforeSwitching() {
  // TODO(xidachen): Try not to reset |paints_before_switching_global_scope_|
  // every frame. For example, if one frame typically has ~5 paint, then we can
  // switch to another global scope after few frames where the accumulated
  // number of paint calls during these frames reached the
  // |paints_before_switching_global_scope_|.
  // TODO(xidachen): Try to set |paints_before_switching_global_scope_|
  // according to the actual paints per frame. For example, if we found that
  // there are typically ~1000 paints in each frame, we'd want to set the number
  // to average at 500.
  return base::RandInt(0, kMaxPaintCountToSwitch - 1);
}

wtf_size_t PaintWorklet::SelectNewGlobalScope() {
  return static_cast<wtf_size_t>(
      base::RandGenerator(kNumGlobalScopesPerThread));
}

scoped_refptr<Image> PaintWorklet::Paint(const String& name,
                                         const ImageResourceObserver& observer,
                                         const FloatSize& container_size,
                                         const CSSStyleValueVector* data,
                                         float device_scale_factor) {
  if (!document_definition_map_.Contains(name))
    return nullptr;

  // Check if the existing document definition is valid or not.
  DocumentPaintDefinition* document_definition =
      document_definition_map_.at(name);
  if (!document_definition)
    return nullptr;

  PaintWorkletGlobalScopeProxy* proxy =
      PaintWorkletGlobalScopeProxy::From(FindAvailableGlobalScope());
  CSSPaintDefinition* paint_definition = proxy->FindDefinition(name);
  if (!paint_definition)
    return nullptr;
  // TODO(crbug.com/946515): Break dependency on LayoutObject.
  const LayoutObject& layout_object =
      static_cast<const LayoutObject&>(observer);
  float zoom = layout_object.StyleRef().EffectiveZoom();

  StylePropertyMapReadOnly* style_map =
      MakeGarbageCollected<PrepopulatedComputedStylePropertyMap>(
          layout_object.GetDocument(), layout_object.StyleRef(),
          paint_definition->NativeInvalidationProperties(),
          paint_definition->CustomInvalidationProperties());
  sk_sp<PaintRecord> paint_record = paint_definition->Paint(
      container_size, zoom, style_map, data, device_scale_factor);
  if (!paint_record)
    return nullptr;
  return PaintGeneratedImage::Create(paint_record, container_size);
}

// static
const char PaintWorklet::kSupplementName[] = "PaintWorklet";

void PaintWorklet::Trace(blink::Visitor* visitor) {
  visitor->Trace(pending_generator_registry_);
  visitor->Trace(proxy_client_);
  Worklet::Trace(visitor);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

void PaintWorklet::RegisterCSSPaintDefinition(const String& name,
                                              CSSPaintDefinition* definition,
                                              ExceptionState& exception_state) {
  if (document_definition_map_.Contains(name)) {
    DocumentPaintDefinition* existing_document_definition =
        document_definition_map_.at(name);
    if (!existing_document_definition)
      return;
    if (!existing_document_definition->RegisterAdditionalPaintDefinition(
            *definition)) {
      document_definition_map_.Set(name, nullptr);
      exception_state.ThrowDOMException(
          DOMExceptionCode::kNotSupportedError,
          "A class with name:'" + name +
              "' was registered with a different definition.");
      return;
    }
    // Notify the generator ready only when register paint is called the
    // second time with the same |name| (i.e. there is already a document
    // definition associated with |name|
    //
    // We are looking for kNumGlobalScopesPerThread number of definitions
    // regiserered from RegisterCSSPaintDefinition and one extra definition from
    // RegisterMainThreadDocumentPaintDefinition if OffMainThreadCSSPaintEnabled
    // is true.
    unsigned required_registered_count =
        RuntimeEnabledFeatures::OffMainThreadCSSPaintEnabled()
            ? kNumGlobalScopesPerThread + 1
            : kNumGlobalScopesPerThread;
    if (existing_document_definition->GetRegisteredDefinitionCount() ==
        required_registered_count)
      pending_generator_registry_->NotifyGeneratorReady(name);
  } else {
    auto document_definition = std::make_unique<DocumentPaintDefinition>(
        definition->NativeInvalidationProperties(),
        definition->CustomInvalidationProperties(),
        definition->InputArgumentTypes(),
        definition->GetPaintRenderingContext2DSettings()->alpha());
    document_definition_map_.insert(name, std::move(document_definition));
  }
}

void PaintWorklet::RegisterMainThreadDocumentPaintDefinition(
    const String& name,
    Vector<CSSPropertyID> native_properties,
    Vector<String> custom_properties,
    Vector<CSSSyntaxDefinition> input_argument_types,
    double alpha) {
  if (document_definition_map_.Contains(name)) {
    DocumentPaintDefinition* document_definition =
        document_definition_map_.at(name);
    if (!document_definition)
      return;
    if (!document_definition->RegisterAdditionalPaintDefinition(
            native_properties, custom_properties, input_argument_types,
            alpha)) {
      document_definition_map_.Set(name, nullptr);
      return;
    }
  } else {
    // Because this method is called cross-thread, |custom_properties| cannot be
    // an AtomicString. Instead, convert to AtomicString now that we are on the
    // main thread.
    Vector<AtomicString> new_custom_properties;
    new_custom_properties.ReserveInitialCapacity(custom_properties.size());
    for (const String& property : custom_properties)
      new_custom_properties.push_back(AtomicString(property));
    auto document_definition = std::make_unique<DocumentPaintDefinition>(
        std::move(native_properties), std::move(new_custom_properties),
        std::move(input_argument_types), alpha);
    document_definition_map_.insert(name, std::move(document_definition));
  }
  DocumentPaintDefinition* document_definition =
      document_definition_map_.at(name);
  // We are looking for kNumGlobalScopesPerThread number of definitions
  // registered from RegisterCSSPaintDefinition and one extra definition from
  // RegisterMainThreadDocumentPaintDefinition
  if (document_definition->GetRegisteredDefinitionCount() ==
      kNumGlobalScopesPerThread + 1)
    pending_generator_registry_->NotifyGeneratorReady(name);
}

bool PaintWorklet::NeedsToCreateGlobalScope() {
  wtf_size_t num_scopes_needed = kNumGlobalScopesPerThread;
  // If we are running off main thread, we will need twice as many global scopes
  if (RuntimeEnabledFeatures::OffMainThreadCSSPaintEnabled())
    num_scopes_needed *= 2;
  return GetNumberOfGlobalScopes() < num_scopes_needed;
}

WorkletGlobalScopeProxy* PaintWorklet::CreateGlobalScope() {
  DCHECK(NeedsToCreateGlobalScope());
  // The main thread global scopes must be created first so that they are at the
  // front of the vector.  This is because SelectNewGlobalScope selects global
  // scopes from the beginning of the vector.  If this code is changed to put
  // the main thread global scopes at the end, then SelectNewGlobalScope must
  // also be changed.
  if (!RuntimeEnabledFeatures::OffMainThreadCSSPaintEnabled() ||
      GetNumberOfGlobalScopes() < kNumGlobalScopesPerThread) {
    return MakeGarbageCollected<PaintWorkletGlobalScopeProxy>(
        To<Document>(GetExecutionContext())->GetFrame(), ModuleResponsesMap(),
        GetNumberOfGlobalScopes() + 1);
  }

  if (!proxy_client_) {
    proxy_client_ = PaintWorkletProxyClient::Create(
        To<Document>(GetExecutionContext()), worklet_id_);
  }

  auto* worker_clients = MakeGarbageCollected<WorkerClients>();
  ProvidePaintWorkletProxyClientTo(worker_clients, proxy_client_);

  PaintWorkletMessagingProxy* proxy =
      MakeGarbageCollected<PaintWorkletMessagingProxy>(GetExecutionContext());
  proxy->Initialize(worker_clients, ModuleResponsesMap());
  return proxy;
}

}  // namespace blink
