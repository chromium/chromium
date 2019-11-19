// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/css_paint_image_generator_impl.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/csspaint/css_paint_definition.h"
#include "third_party/blink/renderer/modules/csspaint/document_paint_definition.h"
#include "third_party/blink/renderer/modules/csspaint/paint_worklet.h"
#include "third_party/blink/renderer/platform/graphics/image.h"

namespace blink {

CSSPaintImageGenerator* CSSPaintImageGeneratorImpl::Create(
    const String& name,
    const Document& document,
    Observer* observer) {
  PaintWorklet* paint_worklet = PaintWorklet::From(*document.domWindow());

  DCHECK(paint_worklet);
  CSSPaintImageGeneratorImpl* generator;
  if (paint_worklet->GetDocumentDefinitionMap().Contains(name)) {
    generator =
        MakeGarbageCollected<CSSPaintImageGeneratorImpl>(paint_worklet, name);
  } else {
    generator = MakeGarbageCollected<CSSPaintImageGeneratorImpl>(
        observer, paint_worklet, name);
    paint_worklet->AddPendingGenerator(name, generator);
  }

  return generator;
}

CSSPaintImageGeneratorImpl::CSSPaintImageGeneratorImpl(
    PaintWorklet* paint_worklet,
    const String& name)
    : CSSPaintImageGeneratorImpl(nullptr, paint_worklet, name) {}

CSSPaintImageGeneratorImpl::CSSPaintImageGeneratorImpl(
    Observer* observer,
    PaintWorklet* paint_worklet,
    const String& name)
    : observer_(observer), paint_worklet_(paint_worklet), name_(name) {}

CSSPaintImageGeneratorImpl::~CSSPaintImageGeneratorImpl() = default;

void CSSPaintImageGeneratorImpl::NotifyGeneratorReady() {
  DCHECK(observer_);
  observer_->PaintImageGeneratorReady();
}

scoped_refptr<Image> CSSPaintImageGeneratorImpl::Paint(
    const ImageResourceObserver& observer,
    const FloatSize& container_size,
    const CSSStyleValueVector* data,
    float device_scale_factor) {
  return paint_worklet_->Paint(name_, observer, container_size, data,
                               device_scale_factor);
}

bool CSSPaintImageGeneratorImpl::HasDocumentDefinition() const {
  return paint_worklet_->GetDocumentDefinitionMap().at(name_);
}

bool CSSPaintImageGeneratorImpl::GetValidDocumentDefinition(
    DocumentPaintDefinition*& definition) const {
  if (!HasDocumentDefinition())
    return false;
  definition = paint_worklet_->GetDocumentDefinitionMap().at(name_);
  // In off-thread CSS Paint, we register CSSPaintDefinition on the worklet
  // thread first. Once the same CSSPaintDefinition is successfully registered
  // to all the paint worklet global scopes, we then post to the main thread and
  // register that CSSPaintDefinition on the main thread. So for the off-thread
  // case, as long as the DocumentPaintDefinition exists in the map, it should
  // be valid.
  if (RuntimeEnabledFeatures::OffMainThreadCSSPaintEnabled()) {
    DCHECK(definition);
    return true;
  }
  if (definition->GetRegisteredDefinitionCount() !=
      PaintWorklet::kNumGlobalScopesPerThread) {
    definition = nullptr;
    return false;
  }
  return definition;
}

unsigned CSSPaintImageGeneratorImpl::GetRegisteredDefinitionCountForTesting()
    const {
  if (!HasDocumentDefinition())
    return 0;
  DocumentPaintDefinition* definition =
      paint_worklet_->GetDocumentDefinitionMap().at(name_);
  return definition->GetRegisteredDefinitionCount();
}

const Vector<CSSPropertyID>&
CSSPaintImageGeneratorImpl::NativeInvalidationProperties() const {
  DEFINE_STATIC_LOCAL(Vector<CSSPropertyID>, empty_vector, ());
  DocumentPaintDefinition* definition;
  if (!GetValidDocumentDefinition(definition))
    return empty_vector;
  return definition->NativeInvalidationProperties();
}

const Vector<AtomicString>&
CSSPaintImageGeneratorImpl::CustomInvalidationProperties() const {
  DEFINE_STATIC_LOCAL(Vector<AtomicString>, empty_vector, ());
  DocumentPaintDefinition* definition;
  if (!GetValidDocumentDefinition(definition))
    return empty_vector;
  return definition->CustomInvalidationProperties();
}

bool CSSPaintImageGeneratorImpl::HasAlpha() const {
  DocumentPaintDefinition* definition;
  if (!GetValidDocumentDefinition(definition))
    return false;
  return definition->alpha();
}

const Vector<CSSSyntaxDefinition>&
CSSPaintImageGeneratorImpl::InputArgumentTypes() const {
  DEFINE_STATIC_LOCAL(Vector<CSSSyntaxDefinition>, empty_vector, ());
  DocumentPaintDefinition* definition;
  if (!GetValidDocumentDefinition(definition))
    return empty_vector;
  return definition->InputArgumentTypes();
}

bool CSSPaintImageGeneratorImpl::IsImageGeneratorReady() const {
  return HasDocumentDefinition();
}

int CSSPaintImageGeneratorImpl::WorkletId() const {
  return paint_worklet_->WorkletId();
}

void CSSPaintImageGeneratorImpl::Trace(blink::Visitor* visitor) {
  visitor->Trace(observer_);
  visitor->Trace(paint_worklet_);
  CSSPaintImageGenerator::Trace(visitor);
}

}  // namespace blink
