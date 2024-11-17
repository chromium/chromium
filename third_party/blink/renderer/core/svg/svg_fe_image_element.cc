/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2010 Dirk Schulze <krit@webkit.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/svg/svg_fe_image_element.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/id_target_observer.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/svg/graphics/filters/svg_fe_image.h"
#include "third_party/blink/renderer/core/svg/svg_animated_preserve_aspect_ratio.h"
#include "third_party/blink/renderer/core/svg/svg_filter_element.h"
#include "third_party/blink/renderer/core/svg/svg_preserve_aspect_ratio.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"

namespace blink {

SVGFEImageElement::SVGFEImageElement(Document& document)
    : SVGFilterPrimitiveStandardAttributes(svg_names::kFEImageTag, document),
      SVGURIReference(this),
      preserve_aspect_ratio_(
          MakeGarbageCollected<SVGAnimatedPreserveAspectRatio>(
              this,
              svg_names::kPreserveAspectRatioAttr)) {}

SVGFEImageElement::~SVGFEImageElement() = default;

void SVGFEImageElement::Trace(Visitor* visitor) const {
  visitor->Trace(preserve_aspect_ratio_);
  visitor->Trace(cached_image_);
  visitor->Trace(target_id_observer_);
  SVGFilterPrimitiveStandardAttributes::Trace(visitor);
  SVGURIReference::Trace(visitor);
  ImageResourceObserver::Trace(visitor);
}

bool SVGFEImageElement::CurrentFrameHasSingleSecurityOrigin() const {
  if (cached_image_) {
    if (Image* image = cached_image_->GetImage())
      return image->CurrentFrameHasSingleSecurityOrigin();
  }
  return true;
}

void SVGFEImageElement::ClearResourceReferences() {
  ClearImageResource();
  UnobserveTarget(target_id_observer_);
  RemoveAllOutgoingReferences();
}

void SVGFEImageElement::FetchImageResource() {
  if (!GetExecutionContext())
    return;

  ResourceLoaderOptions options(GetExecutionContext()->GetCurrentWorld());
  options.initiator_info.name = localName();
  FetchParameters params(
      ResourceRequest(GetDocument().CompleteURL(HrefString())), options);
  cached_image_ = ImageResourceContent::Fetch(params, GetDocument().Fetcher());
  if (cached_image_)
    cached_image_->AddObserver(this);
}

void SVGFEImageElement::ClearImageResource() {
  if (!cached_image_)
    return;
  cached_image_->RemoveObserver(this);
  cached_image_ = nullptr;
}

void SVGFEImageElement::Dispose() {
  if (!cached_image_)
    return;
  cached_image_->DidRemoveObserver();
  cached_image_ = nullptr;
}

void SVGFEImageElement::BuildPendingResource() {
  ClearResourceReferences();
  if (!isConnected())
    return;

  Element* target = ObserveTarget(target_id_observer_, *this);
  if (!target) {
    if (!SVGURLReferenceResolver(HrefString(), GetDocument()).IsLocal())
      FetchImageResource();
  } else if (auto* svg_element = DynamicTo<SVGElement>(target)) {
    // Register us with the target in the dependencies map. Any change of
    // hrefElement that leads to relayout/repainting now informs us, so we can
    // react to it.
    AddReferenceTo(svg_element);
  }

  Invalidate();
}

void SVGFEImageElement::SvgAttributeChanged(
    const SvgAttributeChangedParams& params) {
  const QualifiedName& attr_name = params.name;
  if (attr_name == svg_names::kPreserveAspectRatioAttr) {
    Invalidate();
    return;
  }

  if (SVGURIReference::IsKnownAttribute(attr_name)) {
    BuildPendingResource();
    return;
  }

  SVGFilterPrimitiveStandardAttributes::SvgAttributeChanged(params);
}

Node::InsertionNotificationRequest SVGFEImageElement::InsertedInto(
    ContainerNode& root_parent) {
  SVGFilterPrimitiveStandardAttributes::InsertedInto(root_parent);
  BuildPendingResource();
  return kInsertionDone;
}

void SVGFEImageElement::RemovedFrom(ContainerNode& root_parent) {
  SVGFilterPrimitiveStandardAttributes::RemovedFrom(root_parent);
  if (root_parent.isConnected())
    ClearResourceReferences();
}

void SVGFEImageElement::ImageNotifyFinished(ImageResourceContent*) {
  if (!isConnected())
    return;

  Element* parent = parentElement();
  if (!parent || !IsA<SVGFilterElement>(parent) || !parent->GetLayoutObject())
    return;

  if (LayoutObject* layout_object = GetLayoutObject())
    MarkForLayoutAndParentResourceInvalidation(*layout_object);
}

const SVGElement* SVGFEImageElement::TargetElement() const {
  if (cached_image_)
    return nullptr;
  return DynamicTo<SVGElement>(
      TargetElementFromIRIString(HrefString(), GetTreeScope()));
}

FilterEffect* SVGFEImageElement::Build(SVGFilterBuilder*, Filter* filter) {
  if (cached_image_) {
    // Don't use the broken image icon on image loading errors.
    scoped_refptr<Image> image =
        cached_image_->ErrorOccurred() ? nullptr : cached_image_->GetImage();
    return MakeGarbageCollected<FEImage>(
        filter, image, preserve_aspect_ratio_->CurrentValue());
  }
  return MakeGarbageCollected<FEImage>(filter, TargetElement(),
                                       preserve_aspect_ratio_->CurrentValue());
}

bool SVGFEImageElement::TaintsOrigin() const {
  // We always consider a 'href' that references a local element as tainting.
  return !cached_image_ || !cached_image_->IsAccessAllowed();
}

SVGAnimatedPropertyBase* SVGFEImageElement::PropertyFromAttribute(
    const QualifiedName& attribute_name) const {
  if (attribute_name == svg_names::kPreserveAspectRatioAttr) {
    return preserve_aspect_ratio_.Get();
  } else {
    SVGAnimatedPropertyBase* ret =
        SVGURIReference::PropertyFromAttribute(attribute_name);
    if (ret) {
      return ret;
    } else {
      return SVGFilterPrimitiveStandardAttributes::PropertyFromAttribute(
          attribute_name);
    }
  }
}

void SVGFEImageElement::SynchronizeAllSVGAttributes() const {
  SVGAnimatedPropertyBase* attrs[]{preserve_aspect_ratio_.Get()};
  SynchronizeListOfSVGAttributes(attrs);
  SVGURIReference::SynchronizeAllSVGAttributes();
  SVGFilterPrimitiveStandardAttributes::SynchronizeAllSVGAttributes();
}

}  // namespace blink
