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

#include "third_party/blink/public/common/loader/lcp_critical_path_predictor_util.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_image_bitmap_options.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/media_query_matcher.h"
#include "third_party/blink/renderer/core/css/media_values_dynamic.h"
#include "third_party/blink/renderer/core/css/parser/sizes_attribute_parser.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event_dispatch_forbidden_scope.h"
#include "third_party/blink/renderer/core/dom/node_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/attribution_src_loader.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/cross_origin_attribute.h"
#include "third_party/blink/renderer/core/html/forms/form_associated.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/html_dimension.h"
#include "third_party/blink/renderer/core/html/html_image_fallback_helper.h"
#include "third_party/blink/renderer/core/html/html_picture_element.h"
#include "third_party/blink/renderer/core/html/html_source_element.h"
#include "third_party/blink/renderer/core/html/loading_attribute.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/parser/html_srcset_parser.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/lcp_critical_path_predictor/element_locator.h"
#include "third_party/blink/renderer/core/lcp_critical_path_predictor/lcp_critical_path_predictor.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/media_type_names.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_entry.h"
#include "third_party/blink/renderer/core/style/content_data.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image_for_container.h"
#include "third_party/blink/renderer/platform/network/mime/content_type.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"

namespace blink {

class HTMLImageElement::ViewportChangeListener final
    : public MediaQueryListListener {
 public:
  explicit ViewportChangeListener(HTMLImageElement* element)
      : element_(element) {}

  void NotifyMediaQueryChanged() override {
    if (element_)
      element_->NotifyViewportChanged();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(element_);
    MediaQueryListListener::Trace(visitor);
  }

 private:
  Member<HTMLImageElement> element_;
};

HTMLImageElement::HTMLImageElement(Document& document,
                                   const CreateElementFlags flags)
    : HTMLImageElement(document, flags.IsCreatedByParser()) {}

HTMLImageElement::HTMLImageElement(Document& document, bool created_by_parser)
    : HTMLElement(html_names::kImgTag, document),
      ActiveScriptWrappable<HTMLImageElement>({}),
      image_loader_(MakeGarbageCollected<HTMLImageLoader>(this)),
      image_device_pixel_ratio_(1.0f),
      source_(nullptr),
      layout_disposition_(LayoutDisposition::kPrimaryContent),
      form_was_set_by_parser_(false),
      element_created_by_parser_(created_by_parser),
      is_fallback_image_(false),
      is_legacy_format_or_unoptimized_image_(false),
      is_ad_related_(false),
      is_lcp_element_(false),
      is_auto_sized_(false),
      is_predicted_lcp_element_(false) {
  if (blink::LcppScriptObserverEnabled()) {
    if (LocalFrame* frame = document.GetFrame()) {
      if (LCPScriptObserver* script_observer = frame->GetScriptObserver()) {
        // Record scripts that created this HTMLImageElement.
        creator_scripts_ = script_observer->GetExecutingScriptUrls();
      }
    }
  }
}

HTMLImageElement::~HTMLImageElement() = default;

void HTMLImageElement::Trace(Visitor* visitor) const {
  visitor->Trace(image_loader_);
  visitor->Trace(listener_);
  visitor->Trace(form_);
  visitor->Trace(source_);

  HTMLElement::Trace(visitor);
}

void HTMLImageElement::NotifyViewportChanged() {
  // Re-selecting the source URL in order to pick a more fitting resource
  // And update the image's intrinsic dimensions when the viewport changes.
  // Picking of a better fitting resource is UA dependant, not spec required.
  SelectSourceURL(ImageLoader::kUpdateSizeChanged);
}

HTMLImageElement* HTMLImageElement::CreateForJSConstructor(Document& document) {
  HTMLImageElement* image = MakeGarbageCollected<HTMLImageElement>(document);
  image->element_created_by_parser_ = false;
  return image;
}

HTMLImageElement* HTMLImageElement::CreateForJSConstructor(Document& document,
                                                           unsigned width) {
  HTMLImageElement* image = MakeGarbageCollected<HTMLImageElement>(document);
  image->setWidth(width);
  image->element_created_by_parser_ = false;
  return image;
}

HTMLImageElement* HTMLImageElement::CreateForJSConstructor(Document& document,
                                                           unsigned width,
                                                           unsigned height) {
  HTMLImageElement* image = MakeGarbageCollected<HTMLImageElement>(document);
  image->setWidth(width);
  image->setHeight(height);
  image->element_created_by_parser_ = false;
  return image;
}

bool HTMLImageElement::IsPresentationAttribute(
    const QualifiedName& name) const {
  if (name == html_names::kWidthAttr || name == html_names::kHeightAttr ||
      name == html_names::kBorderAttr || name == html_names::kVspaceAttr ||
      name == html_names::kHspaceAttr || name == html_names::kAlignAttr ||
      name == html_names::kValignAttr)
    return true;
  return HTMLElement::IsPresentationAttribute(name);
}

void HTMLImageElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (name == html_names::kWidthAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyID::kWidth, value);
    if (FastHasAttribute(html_names::kHeightAttr)) {
      const AtomicString& height = FastGetAttribute(html_names::kHeightAttr);
      ApplyAspectRatioToStyle(value, height, style);
    }
  } else if (name == html_names::kHeightAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyID::kHeight, value);
    if (FastHasAttribute(html_names::kWidthAttr)) {
      const AtomicString& width = FastGetAttribute(html_names::kWidthAttr);
      ApplyAspectRatioToStyle(width, value, style);
    }
  } else if (name == html_names::kBorderAttr) {
    ApplyBorderAttributeToStyle(value, style);
  } else if (name == html_names::kVspaceAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyID::kMarginTop, value);
    AddHTMLLengthToStyle(style, CSSPropertyID::kMarginBottom, value);
  } else if (name == html_names::kHspaceAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyID::kMarginLeft, value);
    AddHTMLLengthToStyle(style, CSSPropertyID::kMarginRight, value);
  } else if (name == html_names::kAlignAttr) {
    ApplyAlignmentAttributeToStyle(value, style);
  } else if (name == html_names::kValignAttr) {
    AddPropertyToPresentationAttributeStyle(
        style, CSSPropertyID::kVerticalAlign, value);
  } else {
    HTMLElement::CollectStyleForPresentationAttribute(name, value, style);
  }
}

void HTMLImageElement::CollectExtraStyleForPresentationAttribute(
    MutableCSSPropertyValueSet* style) {
  if (!source_)
    return;

  const AtomicString& width = source_->FastGetAttribute(html_names::kWidthAttr);
  const AtomicString& height =
      source_->FastGetAttribute(html_names::kHeightAttr);
  if (!width && !height)
    return;

  if (width) {
    AddHTMLLengthToStyle(style, CSSPropertyID::kWidth, width);
  } else {
    AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kWidth,
                                            CSSValueID::kAuto);
  }

  if (height) {
    AddHTMLLengthToStyle(style, CSSPropertyID::kHeight, height);
  } else {
    AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kHeight,
                                            CSSValueID::kAuto);
  }

  if (width && height) {
    ApplyAspectRatioToStyle(width, height, style);
  } else {
    AddPropertyToPresentationAttributeStyle(style, CSSPropertyID::kAspectRatio,
                                            CSSValueID::kAuto);
  }
}

const AtomicString HTMLImageElement::ImageSourceURL() const {
  return best_fit_image_url_.IsNull() ? FastGetAttribute(html_names::kSrcAttr)
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
  best_fit_image_url_ = candidate.Url();
  float candidate_density = candidate.Density();
  float old_image_device_pixel_ratio = image_device_pixel_ratio_;
  if (candidate_density >= 0)
    image_device_pixel_ratio_ = 1.0 / candidate_density;

  bool intrinsic_sizing_viewport_dependant = false;
  if (candidate.GetResourceWidth() > 0) {
    intrinsic_sizing_viewport_dependant = true;
    UseCounter::Count(GetDocument(), WebFeature::kSrcsetWDescriptor);
  } else if (!candidate.SrcOrigin()) {
    UseCounter::Count(GetDocument(), WebFeature::kSrcsetXDescriptor);
  }

  if (auto* layout_image = DynamicTo<LayoutImage>(GetLayoutObject())) {
    layout_image->SetImageDevicePixelRatio(image_device_pixel_ratio_);

    if (old_image_device_pixel_ratio != image_device_pixel_ratio_)
      layout_image->IntrinsicSizeChanged();
  }

  if (intrinsic_sizing_viewport_dependant) {
    if (!listener_)
      listener_ = MakeGarbageCollected<ViewportChangeListener>(this);

    GetDocument().GetMediaQueryMatcher().AddViewportListener(listener_);
  } else if (listener_) {
    GetDocument().GetMediaQueryMatcher().RemoveViewportListener(listener_);
  }

  if (is_auto_sized_ && HasLazyLoadingAttribute()) {
    GetDocument().ObserveForLazyLoadedAutoSizedImg(this);
  } else {
    GetDocument().UnobserveForLazyLoadedAutoSizedImg(this);
  }
}

void HTMLImageElement::ParseAttribute(
    const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  if (name == html_names::kAltAttr || name == html_names::kTitleAttr) {
    if (UserAgentShadowRoot()) {
      Element* text =
          UserAgentShadowRoot()->getElementById(AtomicString("alttext"));
      String alt_text_content = AltText();
      if (text && text->textContent() != alt_text_content)
        text->setTextContent(alt_text_content);
    }
  } else if (name == html_names::kSrcAttr || name == html_names::kSrcsetAttr ||
             name == html_names::kSizesAttr) {
    SelectSourceURL(ImageLoader::kUpdateIgnorePreviousError);
  } else if (name == html_names::kUsemapAttr) {
    SetIsLink(!params.new_value.IsNull());
  } else if (name == html_names::kReferrerpolicyAttr) {
    network::mojom::ReferrerPolicy new_referrer_policy =
        network::mojom::ReferrerPolicy::kDefault;
    if (!params.new_value.IsNull()) {
      UseCounter::Count(GetDocument(),
                        WebFeature::kHTMLImageElementReferrerPolicyAttribute);

      SecurityPolicy::ReferrerPolicyFromString(
          params.new_value, kSupportReferrerPolicyLegacyKeywords,
          &new_referrer_policy);
    }

    network::mojom::ReferrerPolicy old_referrer_policy =
        network::mojom::ReferrerPolicy::kDefault;
    if (!params.old_value.IsNull()) {
      SecurityPolicy::ReferrerPolicyFromString(
          params.old_value, kSupportReferrerPolicyLegacyKeywords,
          &old_referrer_policy);
    }

    if (new_referrer_policy != old_referrer_policy) {
      GetImageLoader().UpdateFromElement(
          ImageLoader::kUpdateIgnorePreviousError);
    }
  } else if (name == html_names::kDecodingAttr) {
    UseCounter::Count(GetDocument(), WebFeature::kImageDecodingAttribute);
    decoding_mode_ = ParseImageDecodingMode(params.new_value);
  } else if (name == html_names::kLoadingAttr) {
    LoadingAttributeValue loading = GetLoadingAttributeValue(params.new_value);
    if (loading == LoadingAttributeValue::kEager ||
        (loading == LoadingAttributeValue::kAuto)) {
      GetDocument().UnobserveForLazyLoadedAutoSizedImg(this);
      GetImageLoader().LoadDeferredImage();
    }
  } else if (name == html_names::kFetchpriorityAttr) {
    // We only need to keep track of usage here, as the communication of the
    // |fetchPriority| attribute to the loading pipeline takes place in
    // ImageLoader.
    UseCounter::Count(GetDocument(), WebFeature::kPriorityHints);
  } else if (name == html_names::kCrossoriginAttr) {
    // As per an image's relevant mutations [1], we must queue a new loading
    // microtask when the `crossorigin` attribute state has changed. Note that
    // the attribute value can change without the attribute state changing [2].
    //
    // [1]:
    // https://html.spec.whatwg.org/multipage/images.html#relevant-mutations
    // [2]: https://github.com/whatwg/html/issues/4533#issuecomment-483417499
    CrossOriginAttributeValue new_crossorigin_state =
        GetCrossOriginAttributeValue(params.new_value);
    CrossOriginAttributeValue old_crossorigin_state =
        GetCrossOriginAttributeValue(params.old_value);

    if (new_crossorigin_state != old_crossorigin_state) {
      // Update the current state so we can detect future state changes.
      GetImageLoader().UpdateFromElement(
          ImageLoader::kUpdateIgnorePreviousError);
    }
  } else if (name == html_names::kAttributionsrcAttr) {
    LocalDOMWindow* window = GetDocument().domWindow();
    if (window && window->GetFrame()) {
      // Copied from `ImageLoader::DoUpdateFromElement()`.
      network::mojom::ReferrerPolicy referrer_policy =
          network::mojom::ReferrerPolicy::kDefault;
      AtomicString referrer_policy_attribute =
          FastGetAttribute(html_names::kReferrerpolicyAttr);
      if (!referrer_policy_attribute.IsNull()) {
        SecurityPolicy::ReferrerPolicyFromString(
            referrer_policy_attribute, kSupportReferrerPolicyLegacyKeywords,
            &referrer_policy);
      }
      window->GetFrame()->GetAttributionSrcLoader()->Register(params.new_value,
                                                              /*element=*/this,
                                                              referrer_policy);
    }
  } else if (name == html_names::kSharedstoragewritableAttr &&
             RuntimeEnabledFeatures::SharedStorageAPIM118Enabled(
                 GetExecutionContext())) {
    if (!GetExecutionContext()->IsSecureContext()) {
      GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kOther,
          mojom::blink::ConsoleMessageLevel::kError,
          "sharedStorageWritable: sharedStorage operations are only available "
          "in secure contexts."));
    } else if (!params.new_value.IsNull()) {
      UseCounter::Count(GetDocument(),
                        WebFeature::kSharedStorageAPI_Image_Attribute);
    }
  } else {
    HTMLElement::ParseAttribute(params);
  }
}

String HTMLImageElement::AltText() const {
  // lets figure out the alt text.. magic stuff
  // http://www.w3.org/TR/1998/REC-html40-19980424/appendix/notes.html#altgen
  // also heavily discussed by Hixie on bugzilla
  const AtomicString& alt = FastGetAttribute(html_names::kAltAttr);
  if (!alt.IsNull())
    return alt;
  // fall back to title attribute
  return FastGetAttribute(html_names::kTitleAttr);
}

void HTMLImageElement::InvalidateAttributeMapping() {
  EnsureUniqueElementData().SetPresentationAttributeStyleIsDirty(true);
  SetNeedsStyleRecalc(kLocalStyleChange,
                      StyleChangeReasonForTracing::Create(
                          style_change_reason::kPictureSourceChanged));
}

bool HTMLImageElement::SupportedImageType(
    const String& type,
    const HashSet<String>* disabled_image_types) {
  String trimmed_type = ContentType(type).GetType();
  // An empty type attribute is implicitly supported.
  if (trimmed_type.empty())
    return true;
  if (disabled_image_types && disabled_image_types->Contains(trimmed_type)) {
    return false;
  }
  return MIMETypeRegistry::IsSupportedImagePrefixedMIMEType(trimmed_type);
}

bool HTMLImageElement::HasLazyLoadingAttribute() const {
  return GetLoadingAttributeValue(FastGetAttribute(html_names::kLoadingAttr)) ==
         LoadingAttributeValue::kLazy;
}

bool HTMLImageElement::HasSizesAttribute() const {
  return FastHasAttribute(html_names::kSizesAttr);
}

// http://picture.responsiveimages.org/#update-source-set
ImageCandidate HTMLImageElement::FindBestFitImageFromPictureParent() {
  DCHECK(IsMainThread());
  source_ = nullptr;
  auto* picture_parent = DynamicTo<HTMLPictureElement>(parentNode());
  if (!picture_parent)
    return ImageCandidate();
  HashSet<String> disabled_image_types;
  probe::GetDisabledImageTypes(GetExecutionContext(), &disabled_image_types);
  for (Node* child = picture_parent->firstChild(); child;
       child = child->nextSibling()) {
    if (child == this)
      return ImageCandidate();

    auto* source = DynamicTo<HTMLSourceElement>(child);
    if (!source)
      continue;

    if (!source->FastGetAttribute(html_names::kSrcAttr).IsNull()) {
      Deprecation::CountDeprecation(GetExecutionContext(),
                                    WebFeature::kPictureSourceSrc);
    }
    String srcset = source->FastGetAttribute(html_names::kSrcsetAttr);
    if (srcset.empty())
      continue;
    String type = source->FastGetAttribute(html_names::kTypeAttr);
    if (!SupportedImageType(type, &disabled_image_types))
      continue;

    if (!source->MediaQueryMatches())
      continue;

    ImageCandidate candidate = BestFitSourceForSrcsetAttribute(
        GetDocument().DevicePixelRatio(), SourceSize(*source),
        source->FastGetAttribute(html_names::kSrcsetAttr), &GetDocument());
    if (candidate.IsEmpty())
      continue;
    source_ = source;
    return candidate;
  }
  return ImageCandidate();
}

LayoutObject* HTMLImageElement::CreateLayoutObject(const ComputedStyle& style) {
  if (auto* content_image =
          DynamicTo<ImageContentData>(style.GetContentData())) {
    if (!content_image->GetImage()->ErrorOccurred())
      return LayoutObject::CreateObject(this, style);
  }

  switch (layout_disposition_) {
    case LayoutDisposition::kFallbackContent:
      return LayoutObject::CreateBlockFlowOrListItem(this, style);
    case LayoutDisposition::kPrimaryContent: {
      LayoutImage* image = MakeGarbageCollected<LayoutImage>(this);
      image->SetImageResource(MakeGarbageCollected<LayoutImageResource>());
      image->SetImageDevicePixelRatio(image_device_pixel_ratio_);
      return image;
    }
    case LayoutDisposition::kCollapsed:  // Falls through.
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

void HTMLImageElement::AttachLayoutTree(AttachContext& context) {
  HTMLElement::AttachLayoutTree(context);
  if (auto* layout_image = DynamicTo<LayoutImage>(GetLayoutObject())) {
    if (is_fallback_image_) {
      layout_image->ImageResource()->UseBrokenImage();
    }
    GetImageLoader().OnAttachLayoutTree();
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
  bool was_added_to_picture_parent = false;
  if (auto* picture_parent = DynamicTo<HTMLPictureElement>(parentNode())) {
    picture_parent->AddListenerToSourceChildren();
    was_added_to_picture_parent = picture_parent == insertion_point;
  }

  if (was_added_to_picture_parent) {
    SelectSourceURL(ImageLoader::kUpdateIgnorePreviousError);
  } else if (insertion_point.isConnected()) {
    // If the <img> was inserted into the tree, and the image is not
    // potentially available, fallback rendering needs to be triggered.
    if (!GetImageLoader().ImageIsPotentiallyAvailable()) {
      GetImageLoader().NoImageResourceToLoad();
    }
  }

  static const bool is_lcp_script_observer_enabled =
      blink::LcppScriptObserverEnabled();
  if (is_lcp_script_observer_enabled) {
    if (LocalFrame* frame = GetDocument().GetFrame()) {
      if (LCPScriptObserver* script_observer = frame->GetScriptObserver()) {
        // Record scripts that inserted this HTMLImageElement.
        for (auto& url : script_observer->GetExecutingScriptUrls()) {
          creator_scripts_.insert(url);
        }
      }
    }
  }

  static const bool is_image_lcpp_enabled =
      base::FeatureList::IsEnabled(features::kLCPCriticalPathPredictor) &&
      features::
          kLCPCriticalPathPredictorImageLoadPriorityEnabledForHTMLImageElement
              .Get();
  if (is_image_lcpp_enabled) {
    if (LocalFrame* frame = GetDocument().GetFrame()) {
      if (LCPCriticalPathPredictor* lcpp = frame->GetLCPP()) {
        if (lcpp->IsElementMatchingLocator(*this)) {
          this->SetPredictedLcpElement();
        }
      }
    }
  }

  return HTMLElement::InsertedInto(insertion_point);
}

void HTMLImageElement::RemovedFrom(ContainerNode& insertion_point) {
  if (InActiveDocument() && !last_reported_ad_rect_.IsEmpty()) {
    gfx::Rect empty_rect;
    GetDocument().GetFrame()->Client()->OnMainFrameImageAdRectangleChanged(
        this->GetDomNodeId(), empty_rect);
    last_reported_ad_rect_ = empty_rect;
  }

  if (!form_ || NodeTraversal::HighestAncestorOrSelf(*form_.Get()) !=
                    NodeTraversal::HighestAncestorOrSelf(*this))
    ResetFormOwner();
  if (listener_)
    GetDocument().GetMediaQueryMatcher().RemoveViewportListener(listener_);
  bool was_removed_from_parent = !parentNode();
  auto* picture_parent = DynamicTo<HTMLPictureElement>(
      was_removed_from_parent ? &insertion_point : parentNode());
  if (picture_parent) {
    picture_parent->RemoveListenerFromSourceChildren();
    if (was_removed_from_parent)
      SelectSourceURL(ImageLoader::kUpdateIgnorePreviousError);
  }
  HTMLElement::RemovedFrom(insertion_point);
}

unsigned HTMLImageElement::width() {
  if (InActiveDocument()) {
    GetDocument().UpdateStyleAndLayoutForNode(
        this, DocumentUpdateReason::kJavaScript);
  }

  if (!GetLayoutObject()) {
    // check the attribute first for an explicit pixel value
    // TODO(cbiesinger): The attribute could be a float or percentage value...
    unsigned width = 0;
    if (ParseHTMLNonNegativeInteger(FastGetAttribute(html_names::kWidthAttr),
                                    width))
      return width;

    // if the image is available, use its width
    if (ImageResourceContent* image_content = GetImageLoader().GetContent()) {
      return image_content->IntrinsicSize(kRespectImageOrientation).width();
    }
  }

  return LayoutBoxWidth();
}

unsigned HTMLImageElement::height() {
  if (InActiveDocument()) {
    GetDocument().UpdateStyleAndLayoutForNode(
        this, DocumentUpdateReason::kJavaScript);
  }

  if (!GetLayoutObject()) {
    // check the attribute first for an explicit pixel value
    // TODO(cbiesinger): The attribute could be a float or percentage value...
    unsigned height = 0;
    if (ParseHTMLNonNegativeInteger(FastGetAttribute(html_names::kHeightAttr),
                                    height))
      return height;

    // if the image is available, use its height
    if (ImageResourceContent* image_content = GetImageLoader().GetContent()) {
      return image_content->IntrinsicSize(kRespectImageOrientation).height();
    }
  }

  return LayoutBoxHeight();
}

PhysicalSize HTMLImageElement::DensityCorrectedIntrinsicDimensions() const {
  ImageResourceContent* image_content = GetImageLoader().GetContent();
  if (!image_content || !image_content->HasImage())
    return PhysicalSize();

  float pixel_density = image_device_pixel_ratio_;
  if (image_content->HasDevicePixelRatioHeaderValue() &&
      image_content->DevicePixelRatioHeaderValue() > 0)
    pixel_density = 1 / image_content->DevicePixelRatioHeaderValue();

  PhysicalSize natural_size(
      image_content->GetImage()->Size(kRespectImageOrientation));
  natural_size.Scale(pixel_density);
  return natural_size;
}

unsigned HTMLImageElement::naturalWidth() const {
  return DensityCorrectedIntrinsicDimensions().width.ToUnsigned();
}

unsigned HTMLImageElement::naturalHeight() const {
  return DensityCorrectedIntrinsicDimensions().height.ToUnsigned();
}

unsigned HTMLImageElement::LayoutBoxWidth() const {
  LayoutBox* box = GetLayoutBox();
  return box ? AdjustForAbsoluteZoom::AdjustLayoutUnit(box->ContentWidth(),
                                                       *box)
                   .Round()
             : 0;
}

unsigned HTMLImageElement::LayoutBoxHeight() const {
  LayoutBox* box = GetLayoutBox();
  return box ? AdjustForAbsoluteZoom::AdjustLayoutUnit(box->ContentHeight(),
                                                       *box)
                   .Round()
             : 0;
}

bool HTMLImageElement::IsBeingRendered() const {
  // Spec:
  // https://html.spec.whatwg.org/#being-rendered
  // An element is being rendered if it has any associated CSS layout boxes,
  // SVG layout boxes, or some equivalent in other styling languages.
  return GetLayoutBox() != nullptr;
}

bool HTMLImageElement::AllowAutoSizes() const {
  // Spec:
  // https://html.spec.whatwg.org/#allows-auto-sizes
  // An img element allows auto-sizes if:
  // its loading attribute is in the Lazy state, and
  // its sizes attribute's value is "auto" (ASCII case-insensitive),
  // or starts with "auto," (ASCII case-insensitive).
  //
  // Since this is only used by SizesAttributeParser when sizes starts with
  // "auto" is already, it's unnecessary to check it again here.
  return HasLazyLoadingAttribute();
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
  return attribute.GetName() == html_names::kSrcAttr ||
         attribute.GetName() == html_names::kLowsrcAttr ||
         attribute.GetName() == html_names::kLongdescAttr ||
         (attribute.GetName() == html_names::kUsemapAttr &&
          attribute.Value()[0] != '#') ||
         HTMLElement::IsURLAttribute(attribute);
}

bool HTMLImageElement::HasLegalLinkAttribute(const QualifiedName& name) const {
  return name == html_names::kSrcAttr ||
         HTMLElement::HasLegalLinkAttribute(name);
}

void HTMLImageElement::SetIsAdRelated() {
  if (!is_ad_related_ && GetDocument().View()) {
    GetDocument().View()->RegisterForLifecycleNotifications(this);
  }

  is_ad_related_ = true;
}

void HTMLImageElement::DidFinishLifecycleUpdate(
    const LocalFrameView& local_frame_view) {
  DCHECK(is_ad_related_);

  // Scope to the outermost frame to avoid counting image ads that are (likely)
  // already in ad iframes.
  LocalFrame* frame = GetDocument().GetFrame();
  if (!frame || !frame->View() || !frame->IsOutermostMainFrame()) {
    return;
  }

  gfx::Rect rect_to_report;
  if (LayoutObject* r = GetLayoutObject()) {
    gfx::Rect rect_in_viewport = r->AbsoluteBoundingBoxRect();

    // Exclude image ads that are invisible or too small (e.g. tracking pixels).
    if (rect_in_viewport.width() > 1 && rect_in_viewport.height() > 1) {
      if (!image_ad_use_counter_recorded_) {
        UseCounter::Count(GetDocument(), WebFeature::kImageAd);
        image_ad_use_counter_recorded_ = true;
      }

      rect_to_report =
          rect_in_viewport + frame->View()->LayoutViewport()->ScrollOffsetInt();
    }
  }

  if (last_reported_ad_rect_ != rect_to_report) {
    frame->Client()->OnMainFrameImageAdRectangleChanged(this->GetDomNodeId(),
                                                        rect_to_report);
    last_reported_ad_rect_ = rect_to_report;
  }
}

bool HTMLImageElement::draggable() const {
  // Image elements are draggable by default.
  return !EqualIgnoringASCIICase(FastGetAttribute(html_names::kDraggableAttr),
                                 "false");
}

void HTMLImageElement::setHeight(unsigned value) {
  SetUnsignedIntegralAttribute(html_names::kHeightAttr, value);
}

void HTMLImageElement::setWidth(unsigned value) {
  SetUnsignedIntegralAttribute(html_names::kWidthAttr, value);
}

int HTMLImageElement::x() const {
  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);
  LayoutObject* r = GetLayoutObject();
  if (!r)
    return 0;

  PhysicalOffset abs_pos =
      r->LocalToAbsolutePoint(PhysicalOffset(), kIgnoreTransforms);
  return abs_pos.left.ToInt();
}

int HTMLImageElement::y() const {
  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);
  LayoutObject* r = GetLayoutObject();
  if (!r)
    return 0;

  PhysicalOffset abs_pos =
      r->LocalToAbsolutePoint(PhysicalOffset(), kIgnoreTransforms);
  return abs_pos.top.ToInt();
}

ScriptPromise<IDLUndefined> HTMLImageElement::decode(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  return GetImageLoader().Decode(script_state, exception_state);
}

bool HTMLImageElement::complete() const {
  return GetImageLoader().ImageComplete();
}

void HTMLImageElement::OnResize() {
  if (is_auto_sized_ && HasLazyLoadingAttribute()) {
    SelectSourceURL(ImageLoader::kUpdateSizeChanged);
  }
}

void HTMLImageElement::DidMoveToNewDocument(Document& old_document) {
  GetImageLoader().ElementDidMoveToNewDocument();
  HTMLElement::DidMoveToNewDocument(old_document);
  SelectSourceURL(ImageLoader::kUpdateIgnorePreviousError);
}

bool HTMLImageElement::IsServerMap() const {
  if (!FastHasAttribute(html_names::kIsmapAttr))
    return false;

  const AtomicString& usemap = FastGetAttribute(html_names::kUsemapAttr);

  // If the usemap attribute starts with '#', it refers to a map element in
  // the document.
  if (usemap[0] == '#')
    return false;

  return GetDocument()
      .CompleteURL(StripLeadingAndTrailingHTMLSpaces(usemap))
      .IsEmpty();
}

Image* HTMLImageElement::ImageContents() {
  if (!GetImageLoader().ImageComplete() || !GetImageLoader().GetContent())
    return nullptr;

  return GetImageLoader().GetContent()->GetImage();
}

bool HTMLImageElement::IsInteractiveContent() const {
  return FastHasAttribute(html_names::kUsemapAttr);
}

gfx::SizeF HTMLImageElement::DefaultDestinationSize(
    const gfx::SizeF& default_object_size,
    const RespectImageOrientationEnum respect_orientation) const {
  ImageResourceContent* image_content = CachedImage();
  if (!image_content || !image_content->HasImage())
    return gfx::SizeF();

  Image* image = image_content->GetImage();
  if (auto* svg_image = DynamicTo<SVGImage>(image)) {
    const SVGImageViewInfo* view_info =
        SVGImageForContainer::CreateViewInfo(*svg_image, *this);
    return SVGImageForContainer::ConcreteObjectSize(*svg_image, view_info,
                                                    default_object_size);
  }

  PhysicalSize size(image->Size(respect_orientation));
  if (GetLayoutObject() && GetLayoutObject()->IsLayoutImage() &&
      image->HasIntrinsicSize())
    size.Scale(To<LayoutImage>(GetLayoutObject())->ImageDevicePixelRatio());
  return gfx::SizeF(size);
}

struct SourceSizeValueResult {
  bool has_attribute{};
  float value{};
  bool is_auto{};
};

static SourceSizeValueResult SourceSizeValue(const Element* element,
                                             Document& current_document) {
  SourceSizeValueResult result;

  auto* img = DynamicTo<HTMLImageElement>(element);

  if (!img) {
    // Lookup the <img> from the parent <picture>. The content model for
    // <picture> is "zero or more source elements, followed by one img element,
    // optionally intermixed with script-supporting elements."
    // https://html.spec.whatwg.org/multipage/embedded-content.html#the-picture-element
    if (auto* picture = DynamicTo<HTMLPictureElement>(element->parentNode())) {
      img = Traversal<HTMLImageElement>::LastChild(*picture);
    }
  }

  String sizes = element->FastGetAttribute(html_names::kSizesAttr);
  if (sizes.IsNull() && img != element && img && img->AllowAutoSizes() &&
      img->FastGetAttribute(html_names::kSizesAttr)
          .StartsWithIgnoringASCIICase("auto")) {
    // Spec:
    // https://html.spec.whatwg.org/#the-source-element
    // If the img element allows auto-sizes, then the sizes attribute can be
    // omitted on previous sibling source elements. In such cases, it is
    // equivalent to specifying auto.
    sizes = "auto";
  }
  result.has_attribute = !sizes.IsNull();
  if (result.has_attribute) {
    UseCounter::Count(current_document, WebFeature::kSizes);
  }

  SizesAttributeParser sizes_attribute_parser{
      MediaValuesDynamic::Create(current_document), sizes,
      current_document.GetExecutionContext(), img};

  result.value = sizes_attribute_parser.Size();
  result.is_auto = sizes_attribute_parser.IsAuto();

  if (result.is_auto) {
    if (img) {
      if (img->HasLazyLoadingAttribute()) {
        UseCounter::Count(current_document, WebFeature::kAutoSizesLazy);
      } else {
        UseCounter::Count(current_document, WebFeature::kAutoSizesNonLazy);
      }
    }
  }

  return result;
}

std::optional<float> HTMLImageElement::GetResourceWidth() const {
  std::optional<float> resource_width;
  Element* element = source_.Get();
  const SourceSizeValueResult source_size_val_res =
      SourceSizeValue(element ? element : this, GetDocument());
  if (source_size_val_res.has_attribute) {
    resource_width = source_size_val_res.value;
  }

  return resource_width;
}

float HTMLImageElement::SourceSize(Element& element) {
  const SourceSizeValueResult source_size_val_res =
      SourceSizeValue(&element, GetDocument());

  is_auto_sized_ = source_size_val_res.is_auto;

  if (is_auto_sized_ && HasLazyLoadingAttribute()) {
    GetDocument().ObserveForLazyLoadedAutoSizedImg(this);
  } else {
    GetDocument().UnobserveForLazyLoadedAutoSizedImg(this);
  }

  return source_size_val_res.value;
}

void HTMLImageElement::ForceReload() const {
  GetImageLoader().UpdateFromElement(ImageLoader::kUpdateForcedReload);
}

void HTMLImageElement::SelectSourceURL(
    ImageLoader::UpdateFromElementBehavior behavior) {
  if (!GetDocument().IsActive())
    return;

  HTMLSourceElement* old_source = source_;
  ImageCandidate candidate = FindBestFitImageFromPictureParent();
  if (candidate.IsEmpty()) {
    const float source_size{SourceSize(*this)};

    candidate = BestFitSourceForImageAttributes(
        GetDocument().DevicePixelRatio(), source_size,
        FastGetAttribute(html_names::kSrcAttr),
        FastGetAttribute(html_names::kSrcsetAttr), &GetDocument());
  }
  if (old_source != source_)
    InvalidateAttributeMapping();
  AtomicString old_url = best_fit_image_url_;
  SetBestFitURLAndDPRFromImageCandidate(candidate);

  // Step 5 in
  // https://html.spec.whatwg.org/multipage/images.html#reacting-to-environment-changes
  // Deliberately not compliant and avoiding checking image density, to avoid
  // spurious downloads. See https://github.com/whatwg/html/issues/4646
  if (behavior != HTMLImageLoader::kUpdateSizeChanged ||
      best_fit_image_url_ != old_url) {
    GetImageLoader().UpdateFromElement(behavior);
  }

  if (GetImageLoader().ImageIsPotentiallyAvailable())
    EnsurePrimaryContent();
  else
    EnsureCollapsedOrFallbackContent();
}

void HTMLImageElement::StartLoadingImageDocument(
    ImageResourceContent* image_content) {
  // This element is being used to load an image in an ImageDocument. The
  // provided ImageResource is owned/managed by the ImageDocumentParser. Set it
  // on our ImageLoader and then update the 'src' attribute to reflect the URL
  // of the image. This latter step will also initiate the load from the
  // ImageLoader's PoV.
  GetImageLoader().SetImageDocumentContent(image_content);
  setAttribute(html_names::kSrcAttr, AtomicString(image_content->Url()));
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
  std::optional<ResourceError> error =
      image_content ? image_content->GetResourceError() : std::nullopt;
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

void HTMLImageElement::SetAutoSizesUsecounter() {
  if (listener_ && HasLazyLoadingAttribute()) {
    UseCounter::Count(
        GetDocument(),
        HasSizesAttribute()
            ? WebFeature::kViewportDependentLazyLoadedImageWithSizesAttribute
            : WebFeature::
                  kViewportDependentLazyLoadedImageWithoutSizesAttribute);
  }
}

void HTMLImageElement::SetLayoutDisposition(
    LayoutDisposition layout_disposition,
    bool force_reattach) {
  if (layout_disposition_ == layout_disposition && !force_reattach)
    return;

  DCHECK(!GetDocument().InStyleRecalc());

  layout_disposition_ = layout_disposition;
  if (layout_disposition == LayoutDisposition::kFallbackContent) {
    SetHasCustomStyleCallbacks();
  } else {
    UnsetHasCustomStyleCallbacks();
  }

  if (layout_disposition_ == LayoutDisposition::kFallbackContent) {
    EventDispatchForbiddenScope::AllowUserAgentEvents allow_events;
    EnsureUserAgentShadowRoot();
  }

  // ComputedStyle depends on layout_disposition_. Trigger recalc.
  SetNeedsStyleRecalc(
      kLocalStyleChange,
      StyleChangeReasonForTracing::Create(style_change_reason::kUseFallback));
  // LayoutObject type depends on layout_disposition_. Trigger re-attach.
  SetForceReattachLayoutTree();
}

void HTMLImageElement::AdjustStyle(ComputedStyleBuilder& builder) {
  DCHECK_EQ(layout_disposition_, LayoutDisposition::kFallbackContent);
  HTMLImageFallbackHelper::AdjustHostStyle(*this, builder);
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
