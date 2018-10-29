/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2010 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/bindings/core/v8/script_event_listener.h"
#include "third_party/blink/renderer/core/css/media_query_matcher.h"
#include "third_party/blink/renderer/core/css/media_values_dynamic.h"
#include "third_party/blink/renderer/core/css/parser/sizes_attribute_parser.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css_property_names.h"
#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatch_forbidden_scope.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/forms/form_associated.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/html_image_fallback_helper.h"
#include "third_party/blink/renderer/core/html/html_picture_element.h"
#include "third_party/blink/renderer/core/html/html_source_element.h"
#include "third_party/blink/renderer/core/html/media/media_element_parser_helpers.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/parser/html_srcset_parser.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap_options.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_object_factory.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/media_type_names.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/style/content_data.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image_for_container.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_url.h"
#include "third_party/blink/renderer/platform/network/mime/content_type.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"

namespace blink {

using namespace HTMLNames;

class HTMLImageElement::ViewportChangeListener final
    : public MediaQueryListListener {
 public:
  static ViewportChangeListener* Create(HTMLImageElement* element) {
    return new ViewportChangeListener(element);
  }

  void NotifyMediaQueryChanged() override {
    if (element_)
      element_->NotifyViewportChanged();
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(element_);
    MediaQueryListListener::Trace(visitor);
  }

 private:
  explicit ViewportChangeListener(HTMLImageElement* element)
      : element_(element) {}
  Member<HTMLImageElement> element_;
};

HTMLImageElement::HTMLImageElement(Document& document, bool created_by_parser)
    : HTMLElement(imgTag, document),
      image_loader_(HTMLImageLoader::Create(this)),
      image_device_pixel_ratio_(1.0f),
      source_(nullptr),
      layout_disposition_(LayoutDisposition::kPrimaryContent),
      form_was_set_by_parser_(false),
      element_created_by_parser_(created_by_parser),
      is_fallback_image_(false),
      should_invert_color_(false),
      sizes_set_width_(false),
      referrer_policy_(kReferrerPolicyDefault) {
  SetHasCustomStyleCallbacks();
  if (media_element_parser_helpers::IsMediaElement(this) &&
      !media_element_parser_helpers::IsUnsizedMediaEnabled(document)) {
    is_default_overridden_intrinsic_size_ = true;
    overridden_intrinsic_size_ =
        IntSize(LayoutReplaced::kDefaultWidth, LayoutReplaced::kDefaultHeight);
  }
}

HTMLImageElement* HTMLImageElement::Create(Document& document) {
  return new HTMLImageElement(document);
}

HTMLImageElement* HTMLImageElement::Create(Document& document,
                                           const CreateElementFlags flags) {
  return new HTMLImageElement(document, flags.IsCreatedByParser());
}

HTMLImageElement::~HTMLImageElement() = default;

void HTMLImageElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(image_loader_);
  visitor->Trace(listener_);
  visitor->Trace(form_);
  visitor->Trace(source_);
  HTMLElement::Trace(visitor);
}

const HashSet<AtomicString>& HTMLImageElement::GetCheckedAttributeNames()
    const {
  DEFINE_STATIC_LOCAL(HashSet<AtomicString>, attribute_set, ({"src"}));
  return attribute_set;
}

void HTMLImageElement::NotifyViewportChanged() {
  // Re-selecting the source URL in order to pick a more fitting resource
  // And update the image's intrinsic dimensions when the viewport changes.
  // Picking of a better fitting resource is UA dependant, not spec required.
  SelectSourceURL(ImageLoader::kUpdateSizeChanged);
}

HTMLImageElement* HTMLImageElement::CreateForJSConstructor(Document& document) {
  HTMLImageElement* image = new HTMLImageElement(document);
  image->element_created_by_parser_ = false;
  return image;
}

HTMLImageElement* HTMLImageElement::CreateForJSConstructor(Document& document,
                                                           unsigned width) {
  HTMLImageElement* image = new HTMLImageElement(document);
  image->setWidth(width);
  image->element_created_by_parser_ = false;
  return image;
}

HTMLImageElement* HTMLImageElement::CreateForJSConstructor(Document& document,
                                                           unsigned width,
                                                           unsigned height) {
  HTMLImageElement* image = new HTMLImageElement(document);
  image->setWidth(width);
  image->setHeight(height);
  image->element_created_by_parser_ = false;
  return image;
}

bool HTMLImageElement::IsPresentationAttribute(
    const QualifiedName& name) const {
  if (name == widthAttr || name == heightAttr || name == borderAttr ||
      name == vspaceAttr || name == hspaceAttr || name == alignAttr ||
      name == valignAttr)
    return true;
  return HTMLElement::IsPresentationAttribute(name);
}

void HTMLImageElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (name == widthAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyWidth, value);
  } else if (name == heightAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyHeight, value);
  } else if (name == borderAttr) {
    ApplyBorderAttributeToStyle(value, style);
  } else if (name == vspaceAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyMarginTop, value);
    AddHTMLLengthToStyle(style, CSSPropertyMarginBottom, value);
  } else if (name == hspaceAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyMarginLeft, value);
    AddHTMLLengthToStyle(style, CSSPropertyMarginRight, value);
  } else if (name == alignAttr) {
    ApplyAlignmentAttributeToStyle(value, style);
  } else if (name == valignAttr) {
    AddPropertyToPresentationAttributeStyle(style, CSSPropertyVerticalAlign,
                                            value);
  } else {
    HTMLElement::CollectStyleForPresentationAttribute(name, value, style);
  }
}

const AtomicString HTMLImageElement::ImageSourceURL() const {
  return best_fit_image_url_.IsNull() ? FastGetAttribute(srcAttr)
                                      : best_fit_image_url_;
}

HTMLFormElement* HTMLImageElement::formOwner() const {
  return form_.Get();
}

void HTMLImageElement::FormRemovedFromTree(const Node& form_root) {
  DCHECK(form_);
  if (NodeTraversal::HighestAncestorOrSelf(*this) != form_root)
    ResetFormOwner();
}

void HTMLImageElement::ResetFormOwner() {
  form_was_set_by_parser_ = false;
  HTMLFormElement* nearest_form = FindFormAncestor();
  if (form_) {
    if (nearest_form == form_.Get())
      return;
    form_->Disassociate(*this);
  }
  if (nearest_form) {
    form_ = nearest_form;
    form_->Associate(*this);
  } else {
    form_ = nullptr;
  }
}

void HTMLImageElement::SetBestFitURLAndDPRFromImageCandidate(
    const ImageCandidate& candidate) {
  sizes_set_width_ = false;
  best_fit_image_url_ = candidate.Url();
  float candidate_density = candidate.Density();
  float old_image_device_pixel_ratio = image_device_pixel_ratio_;
  if (candidate_density >= 0)
    image_device_pixel_ratio_ = 1.0 / candidate_density;

  bool intrinsic_sizing_viewport_dependant = false;
  if (candidate.GetResourceWidth() > 0) {
    intrinsic_sizing_viewport_dependant = true;
    sizes_set_width_ = true;
    UseCounter::Count(GetDocument(), WebFeature::kSrcsetWDescriptor);
  } else if (!candidate.SrcOrigin()) {
    UseCounter::Count(GetDocument(), WebFeature::kSrcsetXDescriptor);
  }
  if (GetLayoutObject() && GetLayoutObject()->IsImage()) {
    ToLayoutImage(GetLayoutObject())
        ->SetImageDevicePixelRatio(image_device_pixel_ratio_);

    if (old_image_device_pixel_ratio != image_device_pixel_ratio_)
      ToLayoutImage(GetLayoutObject())->IntrinsicSizeChanged();
  }

  if (intrinsic_sizing_viewport_dependant) {
    if (!listener_)
      listener_ = ViewportChangeListener::Create(this);

    GetDocument().GetMediaQueryMatcher().AddViewportListener(listener_);
  } else if (listener_) {
    GetDocument().GetMediaQueryMatcher().RemoveViewportListener(listener_);
  }
}

void HTMLImageElement::ParseAttribute(
    const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  if (name == altAttr || name == titleAttr) {
    if (UserAgentShadowRoot()) {
      Element* text = UserAgentShadowRoot()->getElementById("alttext");
      String alt_text_content = AltText();
      if (text && text->textContent() != alt_text_content)
        text->setTextContent(alt_text_content);
    }
  } else if (name == srcAttr || name == srcsetAttr || name == sizesAttr) {
    SelectSourceURL(ImageLoader::kUpdateIgnorePreviousError);
  } else if (name == usemapAttr) {
    SetIsLink(!params.new_value.IsNull());
  } else if (name == referrerpolicyAttr) {
    referrer_policy_ = kReferrerPolicyDefault;
    if (!params.new_value.IsNull()) {
      SecurityPolicy::ReferrerPolicyFromString(
          params.new_value, kSupportReferrerPolicyLegacyKeywords,
          &referrer_policy_);
      UseCounter::Count(GetDocument(),
                        WebFeature::kHTMLImageElementReferrerPolicyAttribute);
    }
  } else if (name == decodingAttr) {
    UseCounter::Count(GetDocument(), WebFeature::kImageDecodingAttribute);
    decoding_mode_ = ParseImageDecodingMode(params.new_value);
  } else if (name == intrinsicsizeAttr &&
             RuntimeEnabledFeatures::
                 ExperimentalProductivityFeaturesEnabled()) {
    String message;
    bool intrinsic_size_changed =
        media_element_parser_helpers::ParseIntrinsicSizeAttribute(
            params.new_value, this, &overridden_intrinsic_size_,
            &is_default_overridden_intrinsic_size_, &message);
    if (!message.IsEmpty()) {
      GetDocument().AddConsoleMessage(ConsoleMessage::Create(
          kOtherMessageSource, kWarningMessageLevel, message));
    }

    if (intrinsic_size_changed && GetLayoutObject() &&
        GetLayoutObject()->IsLayoutImage())
      ToLayoutImage(GetLayoutObject())->IntrinsicSizeChanged();
  } else if (name == lazyloadAttr &&
             EqualIgnoringASCIICase(params.new_value, "off") &&
             !GetDocument().IsLazyLoadPolicyEnforced()) {
    GetImageLoader().LoadDeferredImage(referrer_policy_);
  } else {
    HTMLElement::ParseAttribute(params);
  }
}

String HTMLImageElement::AltText() const {
  // lets figure out the alt text.. magic stuff
  // http://www.w3.org/TR/1998/REC-html40-19980424/appendix/notes.html#altgen
  // also heavily discussed by Hixie on bugzilla
  const AtomicString& alt = FastGetAttribute(altAttr);
  if (!alt.IsNull())
    return alt;
  // fall back to title attribute
  return FastGetAttribute(titleAttr);
}

static bool SupportedImageType(const String& type) {
  String trimmed_type = ContentType(type).GetType();
  // An empty type attribute is implicitly supported.
  if (trimmed_type.IsEmpty())
    return true;
  return MIMETypeRegistry::IsSupportedImagePrefixedMIMEType(trimmed_type);
}

// http://picture.responsiveimages.org/#update-source-set
ImageCandidate HTMLImageElement::FindBestFitImageFromPictureParent() {
  DCHECK(IsMainThread());
  Node* parent = parentNode();
  source_ = nullptr;
  if (!parent || !IsHTMLPictureElement(*parent))
    return ImageCandidate();
  for (Node* child = parent->firstChild(); child;
       child = child->nextSibling()) {
    if (child == this)
      return ImageCandidate();

    if (!IsHTMLSourceElement(*child))
      continue;

    HTMLSourceElement* source = ToHTMLSourceElement(child);
    if (!source->FastGetAttribute(srcAttr).IsNull()) {
      Deprecation::CountDeprecation(GetDocument(),
                                    WebFeature::kPictureSourceSrc);
    }
    String srcset = source->FastGetAttribute(srcsetAttr);
    if (srcset.IsEmpty())
      continue;
    String type = source->FastGetAttribute(typeAttr);
    if (!type.IsEmpty() && !SupportedImageType(type))
      continue;

    if (!source->MediaQueryMatches())
      continue;

    ImageCandidate candidate = BestFitSourceForSrcsetAttribute(
        GetDocument().DevicePixelRatio(), SourceSize(*source),
        source->FastGetAttribute(srcsetAttr), &GetDocument());
    if (candidate.IsEmpty())
      continue;
    source_ = source;
    return candidate;
  }
  return ImageCandidate();
}

LayoutObject* HTMLImageElement::CreateLayoutObject(const ComputedStyle& style) {
  const ContentData* content_data = style.GetContentData();
  if (content_data && content_data->IsImage()) {
    const StyleImage* content_image =
        ToImageContentData(content_data)->GetImage();
    bool error_occurred = content_image && content_image->CachedImage() &&
                          content_image->CachedImage()->ErrorOccurred();
    if (!error_occurred)
      return LayoutObject::CreateObject(this, style);
  }

  switch (layout_disposition_) {
    case LayoutDisposition::kFallbackContent:
      return LayoutObjectFactory::CreateBlockFlow(*this, style);
    case LayoutDisposition::kPrimaryContent: {
      LayoutImage* image = new LayoutImage(this);
      image->SetImageResource(LayoutImageResource::Create());
      image->SetImageDevicePixelRatio(image_device_pixel_ratio_);
      return image;
    }
    case LayoutDisposition::kCollapsed:  // Falls through.
    default:
      NOTREACHED();
      return nullptr;
  }
}

void HTMLImageElement::AttachLayoutTree(AttachContext& context) {
  HTMLElement::AttachLayoutTree(context);
  if (GetLayoutObject() && GetLayoutObject()->IsImage()) {
    LayoutImage* layout_image = ToLayoutImage(GetLayoutObject());
    LayoutImageResource* layout_image_resource = layout_image->ImageResource();
    if (is_fallback_image_)
      layout_image_resource->UseBrokenImage();

    if (layout_image_resource->HasImage())
      return;

    if (!GetImageLoader().GetContent() && !layout_image_resource->CachedImage())
      return;
    layout_image_resource->SetImageResource(GetImageLoader().GetContent());
  }
}

Node::InsertionNotificationRequest HTMLImageElement::InsertedInto(
    ContainerNode& insertion_point) {
  if (!form_was_set_by_parser_ ||
      NodeTraversal::HighestAncestorOrSelf(insertion_point) !=
          NodeTraversal::HighestAncestorOrSelf(*form_.Get()))
    ResetFormOwner();
  if (listener_)
    GetDocument().GetMediaQueryMatcher().AddViewportListener(listener_);
  Node* parent = parentNode();
  if (parent && IsHTMLPictureElement(*parent))
    ToHTMLPictureElement(parent)->AddListenerToSourceChildren();

  bool image_was_modified = false;
  if (GetDocument().IsActive()) {
    ImageCandidate candidate = FindBestFitImageFromPictureParent();
    if (!candidate.IsEmpty()) {
      SetBestFitURLAndDPRFromImageCandidate(candidate);
      image_was_modified = true;
    }
  }

  if (image_was_modified ||
      GetImageLoader().ShouldUpdateOnInsertedInto(insertion_point)) {
    GetImageLoader().UpdateFromElement(ImageLoader::kUpdateNormal,
                                       referrer_policy_);
  }
  return HTMLElement::InsertedInto(insertion_point);
}

void HTMLImageElement::RemovedFrom(ContainerNode& insertion_point) {
  if (!form_ || NodeTraversal::HighestAncestorOrSelf(*form_.Get()) !=
                    NodeTraversal::HighestAncestorOrSelf(*this))
    ResetFormOwner();
  if (listener_) {
    GetDocument().GetMediaQueryMatcher().RemoveViewportListener(listener_);
    Node* parent = parentNode();
    if (parent && IsHTMLPictureElement(*parent))
      ToHTMLPictureElement(parent)->RemoveListenerFromSourceChildren();
  }
  HTMLElement::RemovedFrom(insertion_point);
}

unsigned HTMLImageElement::width() {
  if (InActiveDocument())
    GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

  if (!GetLayoutObject()) {
    // check the attribute first for an explicit pixel value
    unsigned width = 0;
    if (ParseHTMLNonNegativeInteger(getAttribute(widthAttr), width))
      return width;

    // if the image is available, use its width
    if (ImageResourceContent* image_content = GetImageLoader().GetContent()) {
      return image_content
          ->IntrinsicSize(LayoutObject::ShouldRespectImageOrientation(nullptr))
          .Width();
    }
  }

  return LayoutBoxWidth();
}

unsigned HTMLImageElement::height() {
  if (InActiveDocument())
    GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

  if (!GetLayoutObject()) {
    // check the attribute first for an explicit pixel value
    unsigned height = 0;
    if (ParseHTMLNonNegativeInteger(getAttribute(heightAttr), height))
      return height;

    // if the image is available, use its height
    if (ImageResourceContent* image_content = GetImageLoader().GetContent()) {
      return image_content
          ->IntrinsicSize(LayoutObject::ShouldRespectImageOrientation(nullptr))
          .Height();
    }
  }

  return LayoutBoxHeight();
}

LayoutSize HTMLImageElement::DensityCorrectedIntrinsicDimensions() const {
  IntSize overridden_intrinsic_size = GetOverriddenIntrinsicSize();
  if (!overridden_intrinsic_size.IsEmpty())
    return LayoutSize(overridden_intrinsic_size);
  ImageResourceContent* image_resource = GetImageLoader().GetContent();
  if (!image_resource || !image_resource->HasImage())
    return LayoutSize();

  float pixel_density = image_device_pixel_ratio_;
  if (image_resource->HasDevicePixelRatioHeaderValue() &&
      image_resource->DevicePixelRatioHeaderValue() > 0)
    pixel_density = 1 / image_resource->DevicePixelRatioHeaderValue();

  RespectImageOrientationEnum respect_image_orientation =
      LayoutObject::ShouldRespectImageOrientation(GetLayoutObject());

  LayoutSize natural_size(
      image_resource->IntrinsicSize(respect_image_orientation));
  natural_size.Scale(pixel_density);
  return natural_size;
}

unsigned HTMLImageElement::naturalWidth() const {
  return DensityCorrectedIntrinsicDimensions().Width().ToUnsigned();
}

unsigned HTMLImageElement::naturalHeight() const {
  return DensityCorrectedIntrinsicDimensions().Height().ToUnsigned();
}

unsigned HTMLImageElement::LayoutBoxWidth() const {
  LayoutBox* box = GetLayoutBox();
  return box ? AdjustForAbsoluteZoom::AdjustInt(
                   box->PhysicalContentBoxRect().PixelSnappedWidth(), box)
             : 0;
}

unsigned HTMLImageElement::LayoutBoxHeight() const {
  LayoutBox* box = GetLayoutBox();
  return box ? AdjustForAbsoluteZoom::AdjustInt(
                   box->PhysicalContentBoxRect().PixelSnappedHeight(), box)
             : 0;
}

const String& HTMLImageElement::currentSrc() const {
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/edits.html#dom-img-currentsrc
  // The currentSrc IDL attribute must return the img element's current
  // request's current URL.

  // Return the picked URL string in case of load error.
  if (GetImageLoader().HadError())
    return best_fit_image_url_;
  // Initially, the pending request turns into current request when it is
  // either available or broken. Check for the resource being in error or
  // having an image to determine these states.
  ImageResourceContent* image_content = GetImageLoader().GetContent();
  if (!image_content ||
      (!image_content->ErrorOccurred() && !image_content->HasImage()))
    return g_empty_atom;

  return image_content->Url().GetString();
}

bool HTMLImageElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName() == srcAttr || attribute.GetName() == lowsrcAttr ||
         attribute.GetName() == longdescAttr ||
         (attribute.GetName() == usemapAttr && attribute.Value()[0] != '#') ||
         HTMLElement::IsURLAttribute(attribute);
}

bool HTMLImageElement::HasLegalLinkAttribute(const QualifiedName& name) const {
  return name == srcAttr || HTMLElement::HasLegalLinkAttribute(name);
}

const QualifiedName& HTMLImageElement::SubResourceAttributeName() const {
  return srcAttr;
}

bool HTMLImageElement::draggable() const {
  // Image elements are draggable by default.
  return !DeprecatedEqualIgnoringCase(getAttribute(draggableAttr), "false");
}

void HTMLImageElement::setHeight(unsigned value) {
  SetUnsignedIntegralAttribute(heightAttr, value);
}

IntSize HTMLImageElement::GetOverriddenIntrinsicSize() const {
  if (sizes_set_width_ && !overridden_intrinsic_size_.IsEmpty()) {
    float width = GetResourceWidth().width;
    return IntSize(
        RoundToInt(LayoutUnit(width)),
        RoundToInt(LayoutUnit(
            width * overridden_intrinsic_size_.Height() /
            static_cast<float>(overridden_intrinsic_size_.Width()))));
  }
  return overridden_intrinsic_size_;
}

KURL HTMLImageElement::Src() const {
  return GetDocument().CompleteURL(getAttribute(srcAttr));
}

void HTMLImageElement::SetSrc(const String& value) {
  setAttribute(srcAttr, AtomicString(value));
}

void HTMLImageElement::SetSrc(const USVStringOrTrustedURL& value,
                              ExceptionState& exception_state) {
  setAttribute(srcAttr, value, exception_state);
}

void HTMLImageElement::setWidth(unsigned value) {
  SetUnsignedIntegralAttribute(widthAttr, value);
}

int HTMLImageElement::x() const {
  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();
  LayoutObject* r = GetLayoutObject();
  if (!r)
    return 0;

  // FIXME: This doesn't work correctly with transforms.
  FloatPoint abs_pos = r->LocalToAbsolute();
  return abs_pos.X();
}

int HTMLImageElement::y() const {
  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();
  LayoutObject* r = GetLayoutObject();
  if (!r)
    return 0;

  // FIXME: This doesn't work correctly with transforms.
  FloatPoint abs_pos = r->LocalToAbsolute();
  return abs_pos.Y();
}

ScriptPromise HTMLImageElement::decode(ScriptState* script_state,
                                       ExceptionState& exception_state) {
  return GetImageLoader().Decode(script_state, exception_state);
}

bool HTMLImageElement::complete() const {
  return GetImageLoader().ImageComplete();
}

void HTMLImageElement::DidMoveToNewDocument(Document& old_document) {
  SelectSourceURL(ImageLoader::kUpdateIgnorePreviousError);
  GetImageLoader().ElementDidMoveToNewDocument();
  HTMLElement::DidMoveToNewDocument(old_document);
}

bool HTMLImageElement::IsServerMap() const {
  if (!FastHasAttribute(ismapAttr))
    return false;

  const AtomicString& usemap = FastGetAttribute(usemapAttr);

  // If the usemap attribute starts with '#', it refers to a map element in
  // the document.
  if (usemap[0] == '#')
    return false;

  return GetDocument()
      .CompleteURL(StripLeadingAndTrailingHTMLSpaces(usemap))
      .IsEmpty();
}

Image* HTMLImageElement::ImageContents() {
  if (!GetImageLoader().ImageComplete())
    return nullptr;

  return GetImageLoader().GetContent()->GetImage();
}

bool HTMLImageElement::IsInteractiveContent() const {
  return FastHasAttribute(usemapAttr);
}

FloatSize HTMLImageElement::DefaultDestinationSize(
    const FloatSize& default_object_size) const {
  ImageResourceContent* image_content = CachedImage();
  if (!image_content)
    return FloatSize();

  Image* image = image_content->GetImage();
  if (image->IsSVGImage())
    return ToSVGImage(image)->ConcreteObjectSize(default_object_size);

  LayoutSize size(image_content->IntrinsicSize(
      LayoutObject::ShouldRespectImageOrientation(GetLayoutObject())));
  if (GetLayoutObject() && GetLayoutObject()->IsLayoutImage() &&
      !image->HasRelativeSize())
    size.Scale(ToLayoutImage(GetLayoutObject())->ImageDevicePixelRatio());
  return FloatSize(size);
}

static bool SourceSizeValue(const Element* element,
                            Document& current_document,
                            float& source_size) {
  String sizes = element->FastGetAttribute(sizesAttr);
  bool exists = !sizes.IsNull();
  if (exists)
    UseCounter::Count(current_document, WebFeature::kSizes);
  source_size =
      SizesAttributeParser(MediaValuesDynamic::Create(current_document), sizes)
          .length();
  return exists;
}

FetchParameters::ResourceWidth HTMLImageElement::GetResourceWidth() const {
  FetchParameters::ResourceWidth resource_width;
  Element* element = source_.Get();
  resource_width.is_set = SourceSizeValue(element ? element : this,
                                          GetDocument(), resource_width.width);
  return resource_width;
}

float HTMLImageElement::SourceSize(Element& element) {
  float value;
  // We don't care here if the sizes attribute exists, so we ignore the return
  // value.  If it doesn't exist, we just return the default.
  SourceSizeValue(&element, GetDocument(), value);
  return value;
}

void HTMLImageElement::ForceReload() const {
  GetImageLoader().UpdateFromElement(ImageLoader::kUpdateForcedReload,
                                     referrer_policy_);
}

void HTMLImageElement::SelectSourceURL(
    ImageLoader::UpdateFromElementBehavior behavior) {
  if (!GetDocument().IsActive())
    return;

  ImageCandidate candidate = FindBestFitImageFromPictureParent();
  if (candidate.IsEmpty()) {
    candidate = BestFitSourceForImageAttributes(
        GetDocument().DevicePixelRatio(), SourceSize(*this),
        FastGetAttribute(srcAttr), FastGetAttribute(srcsetAttr),
        &GetDocument());
  }
  SetBestFitURLAndDPRFromImageCandidate(candidate);

  GetImageLoader().UpdateFromElement(behavior, referrer_policy_);

  ImageResourceContent* image_content = GetImageLoader().GetContent();
  // Images such as data: uri's can return immediately and may already have
  // errored out.
  bool image_has_loaded = image_content && !image_content->IsLoading() &&
                          !image_content->ErrorOccurred();
  bool image_still_loading =
      !image_has_loaded && GetImageLoader().HasPendingActivity() &&
      !GetImageLoader().HasPendingError() && !ImageSourceURL().IsEmpty();
  bool image_has_image = image_content && image_content->HasImage();
  bool image_is_document = GetImageLoader().IsLoadingImageDocument() &&
                           image_content && !image_content->ErrorOccurred();

  // Icky special case for deferred images:
  // A deferred image is not loading, does have pending activity, does not
  // have an error, but it does have an ImageResourceContent associated
  // with it, so imageHasLoaded will be true even though the image hasn't
  // actually loaded. Fixing the definition of imageHasLoaded isn't
  // sufficient, because a deferred image does have pending activity, does not
  // have a pending error, and does have a source URL, so if imageHasLoaded
  // was correct, imageStillLoading would become wrong.
  //
  // Instead of dealing with that, there's a separate check that the
  // ImageResourceContent has non-null image data associated with it, which
  // isn't folded into imageHasLoaded above.
  if ((image_has_loaded && image_has_image) || image_still_loading ||
      image_is_document)
    EnsurePrimaryContent();
  else
    EnsureCollapsedOrFallbackContent();
}

void HTMLImageElement::DidAddUserAgentShadowRoot(ShadowRoot&) {
  HTMLImageFallbackHelper::CreateAltTextShadowTree(*this);
}

void HTMLImageElement::EnsureFallbackForGeneratedContent() {
  // The special casing for generated content in CreateLayoutObject breaks the
  // invariant that the layout object attached to this element will always be
  // appropriate for |layout_disposition_|. Force recreate it.
  // TODO(engedy): Remove this hack. See: https://crbug.com/671953.
  SetLayoutDisposition(LayoutDisposition::kFallbackContent,
                       true /* force_reattach */);
}

void HTMLImageElement::EnsureCollapsedOrFallbackContent() {
  if (is_fallback_image_)
    return;

  ImageResourceContent* image_content = GetImageLoader().GetContent();
  base::Optional<ResourceError> error =
      image_content ? image_content->GetResourceError() : base::nullopt;
  SetLayoutDisposition(error && error->ShouldCollapseInitiator()
                           ? LayoutDisposition::kCollapsed
                           : LayoutDisposition::kFallbackContent);
}

void HTMLImageElement::EnsurePrimaryContent() {
  SetLayoutDisposition(LayoutDisposition::kPrimaryContent);
}

bool HTMLImageElement::IsCollapsed() const {
  return layout_disposition_ == LayoutDisposition::kCollapsed;
}

void HTMLImageElement::SetLayoutDisposition(
    LayoutDisposition layout_disposition,
    bool force_reattach) {
  if (layout_disposition_ == layout_disposition && !force_reattach)
    return;

  DCHECK(!GetDocument().InStyleRecalc());

  layout_disposition_ = layout_disposition;

  if (layout_disposition_ == LayoutDisposition::kFallbackContent) {
    EventDispatchForbiddenScope::AllowUserAgentEvents allow_events;
    EnsureUserAgentShadowRoot();
  }
  LazyReattachIfAttached();
}

scoped_refptr<ComputedStyle> HTMLImageElement::CustomStyleForLayoutObject() {
  switch (layout_disposition_) {
    case LayoutDisposition::kPrimaryContent:  // Fall through.
    case LayoutDisposition::kCollapsed:
      return OriginalStyleForLayoutObject();
    case LayoutDisposition::kFallbackContent:
      return HTMLImageFallbackHelper::CustomStyleForAltText(
          *this, ComputedStyle::Clone(*OriginalStyleForLayoutObject()));
    default:
      NOTREACHED();
      return nullptr;
  }
}

void HTMLImageElement::AssociateWith(HTMLFormElement* form) {
  if (form && form->isConnected()) {
    form_ = form;
    form_was_set_by_parser_ = true;
    form_->Associate(*this);
    form_->DidAssociateByParser();
  }
}

}  // namespace blink
