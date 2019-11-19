/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Torch Mobile, Inc. http://www.torchmobile.com/
 * Copyright (C) 2010 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/parser/html_preload_scanner.h"

#include <memory>
#include "base/optional.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/script/script_type.mojom-blink.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_property_name.h"
#include "third_party/blink/renderer/core/css/media_list.h"
#include "third_party/blink/renderer/core/css/media_query_evaluator.h"
#include "third_party/blink/renderer/core/css/media_values_cached.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/parser/sizes_attribute_parser.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/viewport_data.h"
#include "third_party/blink/renderer/core/html/cross_origin_attribute.h"
#include "third_party/blink/renderer/core/html/html_dimension.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_meta_element.h"
#include "third_party/blink/renderer/core/html/link_rel_attribute.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/parser/html_srcset_parser.h"
#include "third_party/blink/renderer/core/html/parser/html_tokenizer.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/importance_attribute.h"
#include "third_party/blink/renderer/core/loader/preload_helper.h"
#include "third_party/blink/renderer/core/loader/subresource_integrity_helper.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trials.h"
#include "third_party/blink/renderer/core/script/script_loader.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/loader/fetch/integrity_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/subresource_integrity.h"
#include "third_party/blink/renderer/platform/network/mime/content_type.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"

namespace blink {

static bool Match(const StringImpl* impl, const QualifiedName& q_name) {
  return impl == q_name.LocalName().Impl();
}

static bool Match(const AtomicString& name, const QualifiedName& q_name) {
  DCHECK(IsMainThread());
  return q_name.LocalName() == name;
}

static bool Match(const String& name, const QualifiedName& q_name) {
  return ThreadSafeMatch(name, q_name);
}

static const StringImpl* TagImplFor(const HTMLToken::DataVector& data) {
  AtomicString tag_name(data);
  const StringImpl* result = tag_name.Impl();
  if (result->IsStatic())
    return result;
  return nullptr;
}

static const StringImpl* TagImplFor(const String& tag_name) {
  const StringImpl* result = tag_name.Impl();
  if (result->IsStatic())
    return result;
  return nullptr;
}

static String InitiatorFor(const StringImpl* tag_impl) {
  DCHECK(tag_impl);
  if (Match(tag_impl, html_names::kImgTag))
    return html_names::kImgTag.LocalName();
  if (Match(tag_impl, html_names::kInputTag))
    return html_names::kInputTag.LocalName();
  if (Match(tag_impl, html_names::kLinkTag))
    return html_names::kLinkTag.LocalName();
  if (Match(tag_impl, html_names::kScriptTag))
    return html_names::kScriptTag.LocalName();
  if (Match(tag_impl, html_names::kVideoTag))
    return html_names::kVideoTag.LocalName();
  NOTREACHED();
  return g_empty_string;
}

static bool MediaAttributeMatches(const MediaValuesCached& media_values,
                                  const String& attribute_value) {
  scoped_refptr<MediaQuerySet> media_queries =
      MediaQuerySet::Create(attribute_value);
  MediaQueryEvaluator media_query_evaluator(media_values);
  return media_query_evaluator.Eval(*media_queries);
}

class TokenPreloadScanner::StartTagScanner {
  STACK_ALLOCATED();

 public:
  StartTagScanner(const StringImpl* tag_impl,
                  MediaValuesCached* media_values,
                  SubresourceIntegrity::IntegrityFeatures features,
                  TokenPreloadScanner::ScannerType scanner_type,
                  bool priority_hints_origin_trial_enabled)
      : tag_impl_(tag_impl),
        link_is_style_sheet_(false),
        link_is_preconnect_(false),
        link_is_preload_(false),
        link_is_modulepreload_(false),
        link_is_import_(false),
        matched_(true),
        input_is_image_(false),
        nomodule_attribute_value_(false),
        source_size_(0),
        source_size_set_(false),
        defer_(FetchParameters::kNoDefer),
        cross_origin_(kCrossOriginAttributeNotSet),
        importance_(mojom::FetchImportanceMode::kImportanceAuto),
        importance_mode_set_(false),
        media_values_(media_values),
        referrer_policy_set_(false),
        referrer_policy_(network::mojom::ReferrerPolicy::kDefault),
        integrity_attr_set_(false),
        integrity_features_(features),
        loading_attr_value_(LoadingAttrValue::kAuto),
        width_attr_dimension_type_(
            HTMLImageElement::LazyLoadDimensionType::kNotAbsolute),
        height_attr_dimension_type_(
            HTMLImageElement::LazyLoadDimensionType::kNotAbsolute),
        inline_style_dimensions_type_(
            HTMLImageElement::LazyLoadDimensionType::kNotAbsolute),
        scanner_type_(scanner_type),
        priority_hints_origin_trial_enabled_(
            priority_hints_origin_trial_enabled) {
    if (Match(tag_impl_, html_names::kImgTag) ||
        Match(tag_impl_, html_names::kSourceTag) ||
        Match(tag_impl_, html_names::kLinkTag)) {
      source_size_ = SizesAttributeParser(media_values_, String()).length();
      return;
    }
    if (!Match(tag_impl_, html_names::kInputTag) &&
        !Match(tag_impl_, html_names::kScriptTag) &&
        !Match(tag_impl_, html_names::kVideoTag))
      tag_impl_ = nullptr;
  }

  enum URLReplacement { kAllowURLReplacement, kDisallowURLReplacement };

  void ProcessAttributes(const HTMLToken::AttributeList& attributes) {
    DCHECK(IsMainThread());
    if (!tag_impl_)
      return;
    for (const HTMLToken::Attribute& html_token_attribute : attributes) {
      AtomicString attribute_name(html_token_attribute.GetName());
      String attribute_value = html_token_attribute.Value8BitIfNecessary();
      ProcessAttribute(attribute_name, attribute_value);
    }
    PostProcessAfterAttributes();
  }

  void ProcessAttributes(
      const Vector<CompactHTMLToken::Attribute>& attributes) {
    if (!tag_impl_)
      return;
    for (const CompactHTMLToken::Attribute& html_token_attribute : attributes)
      ProcessAttribute(html_token_attribute.GetName(),
                       html_token_attribute.Value());
    PostProcessAfterAttributes();
  }

  void PostProcessAfterAttributes() {
    if (Match(tag_impl_, html_names::kImgTag) ||
        (link_is_preload_ && as_attribute_value_ == "image"))
      SetUrlFromImageAttributes();
  }

  void HandlePictureSourceURL(PictureData& picture_data) {
    if (Match(tag_impl_, html_names::kSourceTag) && matched_ &&
        picture_data.source_url.IsEmpty()) {
      // Must create an IsolatedCopy() since the srcset attribute value will get
      // sent back to the main thread between when we set this, and when we
      // process the closing tag which would clear picture_data_. Having any ref
      // to a string we're going to send will fail
      // IsSafeToSendToAnotherThread().
      picture_data.source_url =
          srcset_image_candidate_.ToString().IsolatedCopy();
      picture_data.source_size_set = source_size_set_;
      picture_data.source_size = source_size_;
      picture_data.picked = true;
    } else if (Match(tag_impl_, html_names::kImgTag) &&
               !picture_data.source_url.IsEmpty()) {
      SetUrlToLoad(picture_data.source_url, kAllowURLReplacement);
    }
  }

  std::unique_ptr<PreloadRequest> CreatePreloadRequest(
      const KURL& predicted_base_url,
      const SegmentedString& source,
      const ClientHintsPreferences& client_hints_preferences,
      const PictureData& picture_data,
      const CachedDocumentParameters& document_parameters) {
    PreloadRequest::RequestType request_type =
        PreloadRequest::kRequestTypePreload;
    base::Optional<ResourceType> type;
    if (ShouldPreconnect()) {
      request_type = PreloadRequest::kRequestTypePreconnect;
    } else {
      if (IsLinkRelPreload()) {
        request_type = PreloadRequest::kRequestTypeLinkRelPreload;
        type = ResourceTypeForLinkPreload();
        if (type == base::nullopt)
          return nullptr;
      } else if (IsLinkRelModulePreload()) {
        request_type = PreloadRequest::kRequestTypeLinkRelPreload;
        type = ResourceType::kScript;
      }
      if (!ShouldPreload(type)) {
        return nullptr;
      }
    }

    TextPosition position =
        TextPosition(source.CurrentLine(), source.CurrentColumn());
    FetchParameters::ResourceWidth resource_width;
    float source_size = source_size_;
    bool source_size_set = source_size_set_;
    if (picture_data.picked) {
      source_size_set = picture_data.source_size_set;
      source_size = picture_data.source_size;
    }
    ResourceFetcher::IsImageSet is_image_set =
        (picture_data.picked || !srcset_image_candidate_.IsEmpty())
            ? ResourceFetcher::kImageIsImageSet
            : ResourceFetcher::kImageNotImageSet;

    if (source_size_set) {
      resource_width.width = source_size;
      resource_width.is_set = true;
    }

    if (type == base::nullopt)
      type = GetResourceType();

    // The element's 'referrerpolicy' attribute (if present) takes precedence
    // over the document's referrer policy.
    network::mojom::ReferrerPolicy referrer_policy =
        (referrer_policy_ != network::mojom::ReferrerPolicy::kDefault)
            ? referrer_policy_
            : document_parameters.referrer_policy;
    auto request = PreloadRequest::CreateIfNeeded(
        InitiatorFor(tag_impl_), position, url_to_load_, predicted_base_url,
        type.value(), referrer_policy, PreloadRequest::kDocumentIsReferrer,
        is_image_set, resource_width, client_hints_preferences, request_type);
    if (!request)
      return nullptr;

    if ((Match(tag_impl_, html_names::kScriptTag) &&
         type_attribute_value_ == "module") ||
        IsLinkRelModulePreload()) {
      request->SetScriptType(mojom::ScriptType::kModule);
    }

    request->SetCrossOrigin(cross_origin_);
    request->SetImportance(importance_);
    request->SetNonce(nonce_);
    request->SetCharset(Charset());
    request->SetDefer(defer_);

    LoadingAttrValue effective_loading_attr_value = loading_attr_value_;
    // If the 'lazyload' feature policy is enforced, the attribute value
    // loading='eager' is considered as 'auto'.
    if (effective_loading_attr_value == LoadingAttrValue::kEager &&
        document_parameters.lazyload_policy_enforced) {
      effective_loading_attr_value = LoadingAttrValue::kAuto;
    }
    if (type == ResourceType::kImage) {
      bool is_lazy_load_image_enabled = false;
      switch (effective_loading_attr_value) {
        case LoadingAttrValue::kEager:
          is_lazy_load_image_enabled = false;
          break;
        case LoadingAttrValue::kLazy:
          is_lazy_load_image_enabled =
              document_parameters.lazy_load_image_setting !=
              LocalFrame::LazyLoadImageSetting::kDisabled;
          break;
        case LoadingAttrValue::kAuto:
          if ((width_attr_dimension_type_ ==
                   HTMLImageElement::LazyLoadDimensionType::kAbsoluteSmall &&
               height_attr_dimension_type_ ==
                   HTMLImageElement::LazyLoadDimensionType::kAbsoluteSmall) ||
              inline_style_dimensions_type_ ==
                  HTMLImageElement::LazyLoadDimensionType::kAbsoluteSmall) {
            is_lazy_load_image_enabled = false;
          } else {
            is_lazy_load_image_enabled =
                document_parameters.lazy_load_image_setting ==
                LocalFrame::LazyLoadImageSetting::kEnabledAutomatic;
          }
          break;
      }
      // Do not preload if lazyload is possible but metadata fetch is disabled.
      if (is_lazy_load_image_enabled &&
          !RuntimeEnabledFeatures::LazyImageLoadingMetadataFetchEnabled()) {
        return nullptr;
      }
      // LazyLoad: Do not preload if absolute dimensions are mentioned in width
      // and height attributes or in the inline style, and the dimensions are
      // not small enough.
      if (is_lazy_load_image_enabled &&
          ((width_attr_dimension_type_ ==
                HTMLImageElement::LazyLoadDimensionType::kAbsoluteNotSmall &&
            height_attr_dimension_type_ ==
                HTMLImageElement::LazyLoadDimensionType::kAbsoluteNotSmall) ||
           inline_style_dimensions_type_ ==
               HTMLImageElement::LazyLoadDimensionType::kAbsoluteNotSmall)) {
        return nullptr;
      }
      request->SetIsLazyLoadImageEnabled(is_lazy_load_image_enabled);
    }

    request->SetIntegrityMetadata(integrity_metadata_);

    if (scanner_type_ == ScannerType::kInsertion)
      request->SetFromInsertionScanner(true);

    return request;
  }

 private:
  enum class LoadingAttrValue { kAuto, kLazy, kEager };

  template <typename NameType>
  void ProcessScriptAttribute(const NameType& attribute_name,
                              const String& attribute_value) {
    // FIXME - Don't set crossorigin multiple times.
    if (Match(attribute_name, html_names::kSrcAttr)) {
      SetUrlToLoad(attribute_value, kDisallowURLReplacement);
    } else if (Match(attribute_name, html_names::kCrossoriginAttr)) {
      SetCrossOrigin(attribute_value);
    } else if (Match(attribute_name, html_names::kNonceAttr)) {
      SetNonce(attribute_value);
    } else if (Match(attribute_name, html_names::kAsyncAttr)) {
      SetDefer(FetchParameters::kLazyLoad);
    } else if (Match(attribute_name, html_names::kDeferAttr)) {
      SetDefer(FetchParameters::kLazyLoad);
    } else if (!integrity_attr_set_ &&
               Match(attribute_name, html_names::kIntegrityAttr)) {
      integrity_attr_set_ = true;
      SubresourceIntegrity::ParseIntegrityAttribute(
          attribute_value, integrity_features_, integrity_metadata_);
    } else if (Match(attribute_name, html_names::kTypeAttr)) {
      type_attribute_value_ = attribute_value;
    } else if (Match(attribute_name, html_names::kLanguageAttr)) {
      language_attribute_value_ = attribute_value;
    } else if (Match(attribute_name, html_names::kNomoduleAttr)) {
      nomodule_attribute_value_ = true;
    } else if (!referrer_policy_set_ &&
               Match(attribute_name, html_names::kReferrerpolicyAttr) &&
               !attribute_value.IsNull()) {
      SetReferrerPolicy(attribute_value,
                        kDoNotSupportReferrerPolicyLegacyKeywords);
    } else if (!importance_mode_set_ &&
               Match(attribute_name, html_names::kImportanceAttr) &&
               priority_hints_origin_trial_enabled_) {
      SetImportance(attribute_value);
    }
  }

  template <typename NameType>
  void ProcessImgAttribute(const NameType& attribute_name,
                           const String& attribute_value) {
    if (Match(attribute_name, html_names::kSrcAttr) && img_src_url_.IsNull()) {
      img_src_url_ = attribute_value;
    } else if (Match(attribute_name, html_names::kCrossoriginAttr)) {
      SetCrossOrigin(attribute_value);
    } else if (Match(attribute_name, html_names::kSrcsetAttr) &&
               srcset_attribute_value_.IsNull()) {
      srcset_attribute_value_ = attribute_value;
    } else if (Match(attribute_name, html_names::kSizesAttr) &&
               !source_size_set_) {
      ParseSourceSize(attribute_value);
    } else if (!referrer_policy_set_ &&
               Match(attribute_name, html_names::kReferrerpolicyAttr) &&
               !attribute_value.IsNull()) {
      SetReferrerPolicy(attribute_value, kSupportReferrerPolicyLegacyKeywords);
    } else if (!importance_mode_set_ &&
               Match(attribute_name, html_names::kImportanceAttr) &&
               priority_hints_origin_trial_enabled_) {
      SetImportance(attribute_value);
    } else if (loading_attr_value_ == LoadingAttrValue::kAuto &&
               Match(attribute_name, html_names::kLoadingAttr) &&
               RuntimeEnabledFeatures::LazyImageLoadingEnabled()) {
      loading_attr_value_ =
          EqualIgnoringASCIICase(attribute_value, "eager")
              ? LoadingAttrValue::kEager
              : EqualIgnoringASCIICase(attribute_value, "lazy")
                    ? LoadingAttrValue::kLazy
                    : LoadingAttrValue::kAuto;
    } else if (width_attr_dimension_type_ ==
                   HTMLImageElement::LazyLoadDimensionType::kNotAbsolute &&
               Match(attribute_name, html_names::kWidthAttr) &&
               RuntimeEnabledFeatures::LazyImageLoadingEnabled()) {
      width_attr_dimension_type_ =
          HTMLImageElement::GetAttributeLazyLoadDimensionType(attribute_value);
    } else if (height_attr_dimension_type_ ==
                   HTMLImageElement::LazyLoadDimensionType::kNotAbsolute &&
               Match(attribute_name, html_names::kHeightAttr) &&
               RuntimeEnabledFeatures::LazyImageLoadingEnabled()) {
      height_attr_dimension_type_ =
          HTMLImageElement::GetAttributeLazyLoadDimensionType(attribute_value);
    } else if (inline_style_dimensions_type_ ==
                   HTMLImageElement::LazyLoadDimensionType::kNotAbsolute &&
               Match(attribute_name, html_names::kStyleAttr) &&
               RuntimeEnabledFeatures::LazyImageLoadingEnabled()) {
      CSSParserMode mode =
          media_values_->StrictMode() ? kHTMLStandardMode : kHTMLQuirksMode;
      const ImmutableCSSPropertyValueSet* property_set =
          CSSParser::ParseInlineStyleDeclaration(
              attribute_value, mode, SecureContextMode::kInsecureContext);
      inline_style_dimensions_type_ =
          HTMLImageElement::GetInlineStyleDimensionsType(property_set);
    }
  }

  void SetUrlFromImageAttributes() {
    srcset_image_candidate_ =
        BestFitSourceForSrcsetAttribute(media_values_->DevicePixelRatio(),
                                        source_size_, srcset_attribute_value_);
    SetUrlToLoad(BestFitSourceForImageAttributes(
                     media_values_->DevicePixelRatio(), source_size_,
                     img_src_url_, srcset_image_candidate_),
                 kAllowURLReplacement);
  }

  template <typename NameType>
  void ProcessLinkAttribute(const NameType& attribute_name,
                            const String& attribute_value) {
    // FIXME - Don't set rel/media/crossorigin multiple times.
    if (Match(attribute_name, html_names::kHrefAttr)) {
      SetUrlToLoad(attribute_value, kDisallowURLReplacement);
      // Used in SetUrlFromImageAttributes() when as=image.
      img_src_url_ = attribute_value;
    } else if (Match(attribute_name, html_names::kRelAttr)) {
      LinkRelAttribute rel(attribute_value);
      link_is_style_sheet_ = rel.IsStyleSheet() && !rel.IsAlternate() &&
                             rel.GetIconType() == kInvalidIcon &&
                             !rel.IsDNSPrefetch();
      link_is_preconnect_ = rel.IsPreconnect();
      link_is_preload_ = rel.IsLinkPreload();
      link_is_modulepreload_ = rel.IsModulePreload();
      link_is_import_ = rel.IsImport();
    } else if (Match(attribute_name, html_names::kMediaAttr)) {
      matched_ &= MediaAttributeMatches(*media_values_, attribute_value);
    } else if (Match(attribute_name, html_names::kCrossoriginAttr)) {
      SetCrossOrigin(attribute_value);
    } else if (Match(attribute_name, html_names::kNonceAttr)) {
      SetNonce(attribute_value);
    } else if (Match(attribute_name, html_names::kAsAttr)) {
      as_attribute_value_ = attribute_value.DeprecatedLower();
    } else if (Match(attribute_name, html_names::kTypeAttr)) {
      type_attribute_value_ = attribute_value;
    } else if (!referrer_policy_set_ &&
               Match(attribute_name, html_names::kReferrerpolicyAttr) &&
               !attribute_value.IsNull()) {
      SetReferrerPolicy(attribute_value,
                        kDoNotSupportReferrerPolicyLegacyKeywords);
    } else if (!integrity_attr_set_ &&
               Match(attribute_name, html_names::kIntegrityAttr)) {
      integrity_attr_set_ = true;
      SubresourceIntegrity::ParseIntegrityAttribute(
          attribute_value, integrity_features_, integrity_metadata_);
    } else if (Match(attribute_name, html_names::kImagesrcsetAttr) &&
               srcset_attribute_value_.IsNull()) {
      srcset_attribute_value_ = attribute_value;
    } else if (Match(attribute_name, html_names::kImagesizesAttr) &&
               !source_size_set_) {
      ParseSourceSize(attribute_value);
    } else if (!importance_mode_set_ &&
               Match(attribute_name, html_names::kImportanceAttr) &&
               priority_hints_origin_trial_enabled_) {
      SetImportance(attribute_value);
    }
  }

  template <typename NameType>
  void ProcessInputAttribute(const NameType& attribute_name,
                             const String& attribute_value) {
    // FIXME - Don't set type multiple times.
    if (Match(attribute_name, html_names::kSrcAttr)) {
      SetUrlToLoad(attribute_value, kDisallowURLReplacement);
    } else if (Match(attribute_name, html_names::kTypeAttr)) {
      input_is_image_ = DeprecatedEqualIgnoringCase(attribute_value,
                                                    input_type_names::kImage);
    }
  }

  template <typename NameType>
  void ProcessSourceAttribute(const NameType& attribute_name,
                              const String& attribute_value) {
    if (Match(attribute_name, html_names::kSrcsetAttr) &&
        srcset_image_candidate_.IsEmpty()) {
      srcset_attribute_value_ = attribute_value;
      srcset_image_candidate_ = BestFitSourceForSrcsetAttribute(
          media_values_->DevicePixelRatio(), source_size_, attribute_value);
    } else if (Match(attribute_name, html_names::kSizesAttr) &&
               !source_size_set_) {
      ParseSourceSize(attribute_value);
      if (!srcset_image_candidate_.IsEmpty()) {
        srcset_image_candidate_ = BestFitSourceForSrcsetAttribute(
            media_values_->DevicePixelRatio(), source_size_,
            srcset_attribute_value_);
      }
    } else if (Match(attribute_name, html_names::kMediaAttr)) {
      // FIXME - Don't match media multiple times.
      matched_ &= MediaAttributeMatches(*media_values_, attribute_value);
    } else if (Match(attribute_name, html_names::kTypeAttr)) {
      matched_ &= MIMETypeRegistry::IsSupportedImagePrefixedMIMEType(
          ContentType(attribute_value).GetType());
    }
  }

  template <typename NameType>
  void ProcessVideoAttribute(const NameType& attribute_name,
                             const String& attribute_value) {
    if (Match(attribute_name, html_names::kPosterAttr))
      SetUrlToLoad(attribute_value, kDisallowURLReplacement);
    else if (Match(attribute_name, html_names::kCrossoriginAttr))
      SetCrossOrigin(attribute_value);
  }

  template <typename NameType>
  void ProcessAttribute(const NameType& attribute_name,
                        const String& attribute_value) {
    if (Match(attribute_name, html_names::kCharsetAttr))
      charset_ = attribute_value;

    if (Match(tag_impl_, html_names::kScriptTag))
      ProcessScriptAttribute(attribute_name, attribute_value);
    else if (Match(tag_impl_, html_names::kImgTag))
      ProcessImgAttribute(attribute_name, attribute_value);
    else if (Match(tag_impl_, html_names::kLinkTag))
      ProcessLinkAttribute(attribute_name, attribute_value);
    else if (Match(tag_impl_, html_names::kInputTag))
      ProcessInputAttribute(attribute_name, attribute_value);
    else if (Match(tag_impl_, html_names::kSourceTag))
      ProcessSourceAttribute(attribute_name, attribute_value);
    else if (Match(tag_impl_, html_names::kVideoTag))
      ProcessVideoAttribute(attribute_name, attribute_value);
  }

  void SetUrlToLoad(const String& value, URLReplacement replacement) {
    // We only respect the first src/href, per HTML5:
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html#attribute-name-state
    if (replacement == kDisallowURLReplacement && !url_to_load_.IsEmpty())
      return;
    String url = StripLeadingAndTrailingHTMLSpaces(value);
    if (url.IsEmpty())
      return;
    url_to_load_ = url;
  }

  const String& Charset() const {
    // FIXME: Its not clear that this if is needed, the loader probably ignores
    // charset for image requests anyway.
    if (Match(tag_impl_, html_names::kImgTag) ||
        Match(tag_impl_, html_names::kVideoTag))
      return g_empty_string;
    return charset_;
  }

  base::Optional<ResourceType> ResourceTypeForLinkPreload() const {
    DCHECK(link_is_preload_);
    return PreloadHelper::GetResourceTypeFromAsAttribute(as_attribute_value_);
  }

  ResourceType GetResourceType() const {
    if (Match(tag_impl_, html_names::kScriptTag))
      return ResourceType::kScript;
    if (Match(tag_impl_, html_names::kImgTag) ||
        Match(tag_impl_, html_names::kVideoTag) ||
        (Match(tag_impl_, html_names::kInputTag) && input_is_image_))
      return ResourceType::kImage;
    if (Match(tag_impl_, html_names::kLinkTag) && link_is_style_sheet_)
      return ResourceType::kCSSStyleSheet;
    if (link_is_preconnect_)
      return ResourceType::kRaw;
    if (Match(tag_impl_, html_names::kLinkTag) && link_is_import_)
      return ResourceType::kImportResource;
    NOTREACHED();
    return ResourceType::kRaw;
  }

  bool ShouldPreconnect() const {
    return Match(tag_impl_, html_names::kLinkTag) && link_is_preconnect_ &&
           !url_to_load_.IsEmpty();
  }

  bool IsLinkRelPreload() const {
    return Match(tag_impl_, html_names::kLinkTag) && link_is_preload_ &&
           !url_to_load_.IsEmpty();
  }

  bool IsLinkRelModulePreload() const {
    return Match(tag_impl_, html_names::kLinkTag) && link_is_modulepreload_ &&
           !url_to_load_.IsEmpty();
  }

  bool ShouldPreloadLink(base::Optional<ResourceType>& type) const {
    if (link_is_style_sheet_) {
      return type_attribute_value_.IsEmpty() ||
             MIMETypeRegistry::IsSupportedStyleSheetMIMEType(
                 ContentType(type_attribute_value_).GetType());
    } else if (link_is_preload_) {
      if (type_attribute_value_.IsEmpty())
        return true;
      String type_from_attribute = ContentType(type_attribute_value_).GetType();
      if ((type == ResourceType::kFont &&
           !MIMETypeRegistry::IsSupportedFontMIMEType(type_from_attribute)) ||
          (type == ResourceType::kImage &&
           !MIMETypeRegistry::IsSupportedImagePrefixedMIMEType(
               type_from_attribute)) ||
          (type == ResourceType::kCSSStyleSheet &&
           !MIMETypeRegistry::IsSupportedStyleSheetMIMEType(
               type_from_attribute))) {
        return false;
      }
    } else if (link_is_modulepreload_) {
      return true;
    } else if (!link_is_import_) {
      return false;
    }

    return true;
  }

  bool ShouldPreload(base::Optional<ResourceType>& type) const {
    if (url_to_load_.IsEmpty())
      return false;
    if (!matched_)
      return false;
    if (Match(tag_impl_, html_names::kLinkTag))
      return ShouldPreloadLink(type);
    if (Match(tag_impl_, html_names::kInputTag) && !input_is_image_)
      return false;
    if (Match(tag_impl_, html_names::kScriptTag)) {
      mojom::ScriptType script_type = mojom::ScriptType::kClassic;
      bool is_import_map = false;
      if (!ScriptLoader::IsValidScriptTypeAndLanguage(
              type_attribute_value_, language_attribute_value_,
              ScriptLoader::kAllowLegacyTypeInTypeAttribute, &script_type,
              &is_import_map)) {
        return false;
      }
      if (is_import_map) {
        // External import maps are not yet supported. https://crbug.com/922212
        return false;
      }
      if (ScriptLoader::BlockForNoModule(script_type,
                                         nomodule_attribute_value_)) {
        return false;
      }
    }
    return true;
  }

  void ParseSourceSize(const String& attribute_value) {
    source_size_ =
        SizesAttributeParser(media_values_, attribute_value).length();
    source_size_set_ = true;
  }

  void SetCrossOrigin(const String& cors_setting) {
    cross_origin_ = GetCrossOriginAttributeValue(cors_setting);
  }

  void SetReferrerPolicy(
      const String& attribute_value,
      ReferrerPolicyLegacyKeywordsSupport legacy_keywords_support) {
    referrer_policy_set_ = true;
    SecurityPolicy::ReferrerPolicyFromString(
        attribute_value, legacy_keywords_support, &referrer_policy_);
  }

  void SetImportance(const String& importance) {
    DCHECK(priority_hints_origin_trial_enabled_);
    importance_mode_set_ = true;
    importance_ = GetFetchImportanceAttributeValue(importance);
  }

  void SetNonce(const String& nonce) { nonce_ = nonce; }

  void SetDefer(FetchParameters::DeferOption defer) { defer_ = defer; }

  bool Defer() const { return defer_; }

  const StringImpl* tag_impl_;
  String url_to_load_;
  ImageCandidate srcset_image_candidate_;
  String charset_;
  bool link_is_style_sheet_;
  bool link_is_preconnect_;
  bool link_is_preload_;
  bool link_is_modulepreload_;
  bool link_is_import_;
  bool matched_;
  bool input_is_image_;
  String img_src_url_;
  String srcset_attribute_value_;
  String as_attribute_value_;
  String type_attribute_value_;
  String language_attribute_value_;
  bool nomodule_attribute_value_;
  float source_size_;
  bool source_size_set_;
  FetchParameters::DeferOption defer_;
  CrossOriginAttributeValue cross_origin_;
  mojom::FetchImportanceMode importance_;
  bool importance_mode_set_;
  String nonce_;
  Member<MediaValuesCached> media_values_;
  bool referrer_policy_set_;
  network::mojom::ReferrerPolicy referrer_policy_;
  bool integrity_attr_set_;
  IntegrityMetadataSet integrity_metadata_;
  SubresourceIntegrity::IntegrityFeatures integrity_features_;
  LoadingAttrValue loading_attr_value_;
  HTMLImageElement::LazyLoadDimensionType width_attr_dimension_type_;
  HTMLImageElement::LazyLoadDimensionType height_attr_dimension_type_;
  HTMLImageElement::LazyLoadDimensionType inline_style_dimensions_type_;
  TokenPreloadScanner::ScannerType scanner_type_;
  // For explanation, see TokenPreloadScanner's declaration.
  bool priority_hints_origin_trial_enabled_;
};

TokenPreloadScanner::TokenPreloadScanner(
    const KURL& document_url,
    std::unique_ptr<CachedDocumentParameters> document_parameters,
    const MediaValuesCached::MediaValuesCachedData& media_values_cached_data,
    const ScannerType scanner_type,
    bool priority_hints_origin_trial_enabled)
    : document_url_(document_url),
      in_style_(false),
      in_picture_(false),
      in_script_(false),
      template_count_(0),
      document_parameters_(std::move(document_parameters)),
      media_values_(
          MakeGarbageCollected<MediaValuesCached>(media_values_cached_data)),
      scanner_type_(scanner_type),
      priority_hints_origin_trial_enabled_(priority_hints_origin_trial_enabled),
      did_rewind_(false) {
  DCHECK(document_parameters_.get());
  DCHECK(media_values_.Get());
  DCHECK(document_url.IsValid());
  css_scanner_.SetReferrerPolicy(document_parameters_->referrer_policy);
}

TokenPreloadScanner::~TokenPreloadScanner() = default;

TokenPreloadScannerCheckpoint TokenPreloadScanner::CreateCheckpoint() {
  TokenPreloadScannerCheckpoint checkpoint = checkpoints_.size();
  checkpoints_.push_back(Checkpoint(predicted_base_element_url_, in_style_,
                                    in_script_, template_count_));
  return checkpoint;
}

void TokenPreloadScanner::RewindTo(
    TokenPreloadScannerCheckpoint checkpoint_index) {
  // If this ASSERT fires, checkpointIndex is invalid.
  DCHECK_LT(checkpoint_index, checkpoints_.size());
  const Checkpoint& checkpoint = checkpoints_[checkpoint_index];
  predicted_base_element_url_ = checkpoint.predicted_base_element_url;
  in_style_ = checkpoint.in_style;
  template_count_ = checkpoint.template_count;

  did_rewind_ = true;
  in_script_ = checkpoint.in_script;

  css_scanner_.Reset();
  checkpoints_.clear();
}

void TokenPreloadScanner::Scan(const HTMLToken& token,
                               const SegmentedString& source,
                               PreloadRequestStream& requests,
                               base::Optional<ViewportDescription>* viewport,
                               bool* is_csp_meta_tag) {
  ScanCommon(token, source, requests, viewport, is_csp_meta_tag);
}

void TokenPreloadScanner::Scan(const CompactHTMLToken& token,
                               const SegmentedString& source,
                               PreloadRequestStream& requests,
                               base::Optional<ViewportDescription>* viewport,
                               bool* is_csp_meta_tag) {
  ScanCommon(token, source, requests, viewport, is_csp_meta_tag);
}

static void HandleMetaViewport(
    const String& attribute_value,
    const CachedDocumentParameters* document_parameters,
    MediaValuesCached* media_values,
    base::Optional<ViewportDescription>* viewport) {
  if (!document_parameters->viewport_meta_enabled)
    return;
  ViewportDescription description(ViewportDescription::kViewportMeta);
  HTMLMetaElement::GetViewportDescriptionFromContentAttribute(
      attribute_value, description, nullptr,
      document_parameters->viewport_meta_zero_values_quirk);
  if (viewport)
    *viewport = description;
  FloatSize initial_viewport(media_values->DeviceWidth(),
                             media_values->DeviceHeight());
  PageScaleConstraints constraints = description.Resolve(
      initial_viewport, document_parameters->default_viewport_min_width);
  media_values->OverrideViewportDimensions(constraints.layout_size.Width(),
                                           constraints.layout_size.Height());
}

static void HandleMetaReferrer(const String& attribute_value,
                               CachedDocumentParameters* document_parameters,
                               CSSPreloadScanner* css_scanner) {
  network::mojom::ReferrerPolicy meta_referrer_policy =
      network::mojom::ReferrerPolicy::kDefault;
  if (!attribute_value.IsEmpty() && !attribute_value.IsNull() &&
      SecurityPolicy::ReferrerPolicyFromString(
          attribute_value, kSupportReferrerPolicyLegacyKeywords,
          &meta_referrer_policy)) {
    document_parameters->referrer_policy = meta_referrer_policy;
  }
  css_scanner->SetReferrerPolicy(document_parameters->referrer_policy);
}

template <typename Token>
static void HandleMetaNameAttribute(
    const Token& token,
    CachedDocumentParameters* document_parameters,
    MediaValuesCached* media_values,
    CSSPreloadScanner* css_scanner,
    base::Optional<ViewportDescription>* viewport) {
  const typename Token::Attribute* name_attribute =
      token.GetAttributeItem(html_names::kNameAttr);
  if (!name_attribute)
    return;

  String name_attribute_value(name_attribute->Value());
  const typename Token::Attribute* content_attribute =
      token.GetAttributeItem(html_names::kContentAttr);
  if (!content_attribute)
    return;

  String content_attribute_value(content_attribute->Value());
  if (DeprecatedEqualIgnoringCase(name_attribute_value, "viewport")) {
    HandleMetaViewport(content_attribute_value, document_parameters,
                       media_values, viewport);
    return;
  }

  if (DeprecatedEqualIgnoringCase(name_attribute_value, "referrer")) {
    HandleMetaReferrer(content_attribute_value, document_parameters,
                       css_scanner);
  }
}

template <typename Token>
void TokenPreloadScanner::ScanCommon(
    const Token& token,
    const SegmentedString& source,
    PreloadRequestStream& requests,
    base::Optional<ViewportDescription>* viewport,
    bool* is_csp_meta_tag) {
  if (!document_parameters_->do_html_preload_scanning)
    return;

  switch (token.GetType()) {
    case HTMLToken::kCharacter: {
      if (in_style_) {
        css_scanner_.Scan(token.Data(), source, requests,
                          predicted_base_element_url_);
      }
      return;
    }
    case HTMLToken::kEndTag: {
      const StringImpl* tag_impl = TagImplFor(token.Data());
      if (Match(tag_impl, html_names::kTemplateTag)) {
        if (template_count_)
          --template_count_;
        return;
      }
      if (Match(tag_impl, html_names::kStyleTag)) {
        if (in_style_)
          css_scanner_.Reset();
        in_style_ = false;
        return;
      }
      if (Match(tag_impl, html_names::kScriptTag)) {
        in_script_ = false;
        return;
      }
      if (Match(tag_impl, html_names::kPictureTag)) {
        in_picture_ = false;
        picture_data_.picked = false;
      }
      return;
    }
    case HTMLToken::kStartTag: {
      if (template_count_)
        return;
      const StringImpl* tag_impl = TagImplFor(token.Data());
      if (Match(tag_impl, html_names::kTemplateTag)) {
        ++template_count_;
        return;
      }
      if (Match(tag_impl, html_names::kStyleTag)) {
        in_style_ = true;
        return;
      }
      // Don't early return, because the StartTagScanner needs to look at these
      // too.
      if (Match(tag_impl, html_names::kScriptTag)) {
        in_script_ = true;
      }
      if (Match(tag_impl, html_names::kBaseTag)) {
        // The first <base> element is the one that wins.
        if (!predicted_base_element_url_.IsEmpty())
          return;
        UpdatePredictedBaseURL(token);
        return;
      }
      if (Match(tag_impl, html_names::kMetaTag)) {
        const typename Token::Attribute* equiv_attribute =
            token.GetAttributeItem(html_names::kHttpEquivAttr);
        if (equiv_attribute) {
          String equiv_attribute_value(equiv_attribute->Value());
          if (DeprecatedEqualIgnoringCase(equiv_attribute_value,
                                          "content-security-policy")) {
            *is_csp_meta_tag = true;
          } else if (DeprecatedEqualIgnoringCase(equiv_attribute_value,
                                                 "accept-ch")) {
            const typename Token::Attribute* content_attribute =
                token.GetAttributeItem(html_names::kContentAttr);
            if (content_attribute) {
              client_hints_preferences_.UpdateFromAcceptClientHintsHeader(
                  content_attribute->Value(), document_url_, nullptr);
            }
          }
          return;
        }

        HandleMetaNameAttribute(token, document_parameters_.get(),
                                media_values_.Get(), &css_scanner_, viewport);
      }

      if (Match(tag_impl, html_names::kPictureTag)) {
        in_picture_ = true;
        picture_data_ = PictureData();
        return;
      } else if (!Match(tag_impl, html_names::kSourceTag) &&
                 !Match(tag_impl, html_names::kImgTag)) {
        // If found an "atypical" picture child, don't process it as a picture
        // child.
        in_picture_ = false;
        picture_data_.picked = false;
      }

      StartTagScanner scanner(
          tag_impl, media_values_, document_parameters_->integrity_features,
          scanner_type_, priority_hints_origin_trial_enabled_);
      scanner.ProcessAttributes(token.Attributes());
      // TODO(yoav): ViewportWidth is currently racy and might be zero in some
      // cases, at least in tests. That problem will go away once
      // ParseHTMLOnMainThread lands and MediaValuesCached is eliminated.
      if (in_picture_ && media_values_->ViewportWidth())
        scanner.HandlePictureSourceURL(picture_data_);
      std::unique_ptr<PreloadRequest> request = scanner.CreatePreloadRequest(
          predicted_base_element_url_, source, client_hints_preferences_,
          picture_data_, *document_parameters_);
      if (request) {
        requests.push_back(std::move(request));
      }
      return;
    }
    default: { return; }
  }
}

template <typename Token>
void TokenPreloadScanner::UpdatePredictedBaseURL(const Token& token) {
  DCHECK(predicted_base_element_url_.IsEmpty());
  if (const typename Token::Attribute* href_attribute =
          token.GetAttributeItem(html_names::kHrefAttr)) {
    KURL url(document_url_, StripLeadingAndTrailingHTMLSpaces(
                                href_attribute->Value8BitIfNecessary()));
    predicted_base_element_url_ =
        url.IsValid() && !url.ProtocolIsData() ? url.Copy() : KURL();
  }
}

HTMLPreloadScanner::HTMLPreloadScanner(
    const HTMLParserOptions& options,
    const KURL& document_url,
    std::unique_ptr<CachedDocumentParameters> document_parameters,
    const MediaValuesCached::MediaValuesCachedData& media_values_cached_data,
    const TokenPreloadScanner::ScannerType scanner_type)
    : scanner_(document_url,
               std::move(document_parameters),
               media_values_cached_data,
               scanner_type,
               options.priority_hints_origin_trial_enabled),
      tokenizer_(std::make_unique<HTMLTokenizer>(options)) {}

HTMLPreloadScanner::~HTMLPreloadScanner() = default;

void HTMLPreloadScanner::AppendToEnd(const SegmentedString& source) {
  source_.Append(source);
}

PreloadRequestStream HTMLPreloadScanner::Scan(
    const KURL& starting_base_element_url,
    base::Optional<ViewportDescription>* viewport,
    bool& has_csp_meta_tag) {
  // HTMLTokenizer::updateStateFor only works on the main thread.
  DCHECK(IsMainThread());

  TRACE_EVENT1("blink", "HTMLPreloadScanner::scan", "source_length",
               source_.length());

  // When we start scanning, our best prediction of the baseElementURL is the
  // real one!
  if (!starting_base_element_url.IsEmpty())
    scanner_.SetPredictedBaseElementURL(starting_base_element_url);

  PreloadRequestStream requests;

  while (tokenizer_->NextToken(source_, token_)) {
    if (token_.GetType() == HTMLToken::kStartTag)
      tokenizer_->UpdateStateFor(
          AttemptStaticStringCreation(token_.GetName(), kLikely8Bit));
    bool seen_csp_meta_tag = false;
    scanner_.Scan(token_, source_, requests, viewport, &seen_csp_meta_tag);
    has_csp_meta_tag |= seen_csp_meta_tag;
    token_.Clear();
    // Don't preload anything if a CSP meta tag is found. We should rarely find
    // them here because the HTMLPreloadScanner is only used for the synchronous
    // parsing path.
    if (seen_csp_meta_tag) {
      // Reset the tokenizer, to avoid re-scanning tokens that we are about to
      // start parsing.
      source_.Clear();
      tokenizer_->Reset();
      return requests;
    }
  }

  return requests;
}

CachedDocumentParameters::CachedDocumentParameters(Document* document) {
  DCHECK(IsMainThread());
  DCHECK(document);
  do_html_preload_scanning =
      !document->GetSettings() ||
      document->GetSettings()->GetDoHtmlPreloadScanning();
  default_viewport_min_width =
      document->GetViewportData().ViewportDefaultMinWidth();
  viewport_meta_zero_values_quirk =
      document->GetSettings() &&
      document->GetSettings()->GetViewportMetaZeroValuesQuirk();
  viewport_meta_enabled = document->GetSettings() &&
                          document->GetSettings()->GetViewportMetaEnabled();
  referrer_policy = document->GetReferrerPolicy();
  integrity_features = SubresourceIntegrityHelper::GetFeatures(document);
  lazyload_policy_enforced = document->IsLazyLoadPolicyEnforced();
  if (document->Loader() && document->Loader()->GetFrame()) {
    lazy_load_image_setting =
        document->Loader()->GetFrame()->GetLazyLoadImageSetting();
  } else {
    lazy_load_image_setting = LocalFrame::LazyLoadImageSetting::kDisabled;
  }
}

}  // namespace blink
