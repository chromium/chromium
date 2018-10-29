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
    generator = new CSSPaintImageGeneratorImpl(paint_worklet, name);
  } else {
    generator = new CSSPaintImageGeneratorImpl(observer, paint_worklet, name);
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
    const CSSStyleValueVector* data) {
  return paint_worklet_->Paint(name_, observer, container_size, data);
}

bool CSSPaintImageGeneratorImpl::HasDocumentDefinition() const {
  return paint_worklet_->GetDocumentDefinitionMap().Contains(name_);
}

bool CSSPaintImageGeneratorImpl::GetValidDocumentDefinition(
    DocumentPaintDefinition*& definition) const {
  if (!HasDocumentDefinition())
    return false;
  definition = paint_worklet_->GetDocumentDefinitionMap().at(name_);
  if (definition != kInvalidDocumentPaintDefinition &&
      definition->GetRegisteredDefinitionCount() !=
          PaintWorklet::kNumGlobalScopes) {
    definition = kInvalidDocumentPaintDefinition;
    return false;
  }
  return definition != kInvalidDocumentPaintDefinition;
}

unsigned CSSPaintImageGeneratorImpl::GetRegisteredDefinitionCountForTesting()
    const {
  if (!HasDocumentDefinition())
    return 0;
  DocumentPaintDefinition* definition =
      paint_worklet_->GetDocumentDefinitionMap().at(name_);
  if (definition == kInvalidDocumentPaintDefinition)
    return 0;
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
  return definition->GetPaintRenderingContext2DSettings().alpha();
}

const Vector<CSSSyntaxDescriptor>&
CSSPaintImageGeneratorImpl::InputArgumentTypes() const {
  DEFINE_STATIC_LOCAL(Vector<CSSSyntaxDescriptor>, empty_vector, ());
  DocumentPaintDefinition* definition;
  if (!GetValidDocumentDefinition(definition))
    return empty_vector;
  return definition->InputArgumentTypes();
}

bool CSSPaintImageGeneratorImpl::IsImageGeneratorReady() const {
  return HasDocumentDefinition();
}

void CSSPaintImageGeneratorImpl::Trace(blink::Visitor* visitor) {
  visitor->Trace(observer_);
  visitor->Trace(paint_worklet_);
  CSSPaintImageGenerator::Trace(visitor);
}

}  // namespace blink
