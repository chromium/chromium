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

#include "base/task/sequenced_task_runner.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
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
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/viewport_data.h"
#include "third_party/blink/renderer/core/html/blocking_attribute.h"
#include "third_party/blink/renderer/core/html/client_hints_util.h"
#include "third_party/blink/renderer/core/html/cross_origin_attribute.h"
#include "third_party/blink/renderer/core/html/html_dimension.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_meta_element.h"
#include "third_party/blink/renderer/core/html/link_rel_attribute.h"
#include "third_party/blink/renderer/core/html/loading_attribute.h"
#include "third_party/blink/renderer/core/html/parser/background_html_scanner.h"
#include "third_party/blink/renderer/core/html/parser/html_document_parser.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/parser/html_srcset_parser.h"
#include "third_party/blink/renderer/core/html/parser/html_tokenizer.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/lcp_critical_path_predictor/lcp_critical_path_predictor.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/fetch_priority_attribute.h"
#include "third_party/blink/renderer/core/loader/preload_helper.h"
#include "third_party/blink/renderer/core/loader/subresource_integrity_helper.h"
#include "third_party/blink/renderer/core/loader/web_bundle/script_web_bundle_rule.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/script/script_loader.h"
#include "third_party/blink/renderer/core/script_type_names.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/loader/fetch/client_hints_preferences.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/integrity_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/subresource_integrity.h"
#include "third_party/blink/renderer/platform/network/mime/content_type.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"

namespace blink {

namespace {

bool Match(const AtomicString& name, const QualifiedName& q_name) {
  return q_name.LocalName() == name;
}

String InitiatorFor(const StringImpl* tag_impl, bool link_is_modulepreload) {
  DCHECK(tag_impl);
  if (Match(tag_impl, html_names::kImgTag))
    return html_names::kImgTag.LocalName();
  if (Match(tag_impl, html_names::kInputTag))
    return html_names::kInputTag.LocalName();
  if (Match(tag_impl, html_names::kLinkTag)) {
    if (link_is_modulepreload)
      return fetch_initiator_type_names::kOther;
    return html_names::kLinkTag.LocalName();
  }
  if (Match(tag_impl, html_names::kScriptTag))
    return html_names::kScriptTag.LocalName();
  if (Match(tag_impl, html_names::kVideoTag))
    return html_names::kVideoTag.LocalName();
  NOTREACHED();
  return g_empty_string;
}

bool MediaAttributeMatches(const MediaValuesCached& media_values,
                           const String& attribute_value) {
  // Since this is for preload scanning only, ExecutionContext-based origin
  // trials for media queries are not needed.
  MediaQuerySet* media_queries =
      MediaQuerySet::Create(attribute_value, nullptr);
  MediaQueryEvaluator media_query_evaluator(&media_values);
  return media_query_evaluator.Eval(*media_queries);
}

void ScanScriptWebBundle(
    const String& inline_text,
    const KURL& base_url,
    scoped_refptr<const PreloadRequest::ExclusionInfo>& exclusion_info) {
  auto rule_or_error =
      ScriptWebBundleRule::ParseJson(inline_text, base_url, /*logger*/ nullptr);
  if (!absl::holds_alternative<ScriptWebBundleRule>(rule_or_error))
    return;
  auto& rule = absl::get<ScriptWebBundleRule>(rule_or_error);

  HashSet<KURL> scopes;
  HashSet<KURL> resources;
  if (exclusion_info) {
    scopes = exclusion_info->scopes();
    resources = exclusion_info->resources();
  }

  for (const KURL& scope_url : rule.scope_urls())
    scopes.insert(scope_url);
  for (const KURL& resource_url : rule.resource_urls())
    resources.insert(resource_url);

  exclusion_info = base::MakeRefCounted<PreloadRequest::ExclusionInfo>(
      base_url, std::move(scopes), std::move(resources));
}

void ScanScriptWebBundle(
    const HTMLToken::DataVector& data,
    const KURL& base_url,
    scoped_refptr<const PreloadRequest::ExclusionInfo>& exclusion_info) {
  ScanScriptWebBundle(data.AsString(), base_url, exclusion_info);
}

}  // namespace

bool Match(const StringImpl* impl, const QualifiedName& q_name) {
  return impl == q_name.LocalName().Impl();
}

const StringImpl* TagImplFor(const HTMLToken::DataVector& data) {
  AtomicString tag_name = data.AsAtomicString();
  const StringImpl* result = tag_name.Impl();
  if (result->IsStatic())
    return result;
  return nullptr;
}

class TokenPreloadScanner::StartTagScanner {
  STACK_ALLOCATED();

 public:
  StartTagScanner(const StringImpl* tag_impl,
                  MediaValuesCached* media_values,
                  SubresourceIntegrity::IntegrityFeatures features,
                  TokenPreloadScanner::ScannerType scanner_type,
                  const HashSet<String>* disabled_image_types)
      : tag_impl_(tag_impl),
        media_values_(media_values),
        integrity_features_(features),
        scanner_type_(scanner_type),
        disabled_image_types_(disabled_image_types) {
    if (Match(tag_impl_, html_names::kImgTag) ||
        Match(tag_impl_, html_names::kSourceTag) ||
        Match(tag_impl_, html_names::kLinkTag)) {
      source_size_ =
          SizesAttributeParser(media_values_, String(), nullptr).length();
      return;
    }
    if (!Match(tag_impl_, html_names::kInputTag) &&
        !Match(tag_impl_, html_names::kScriptTag) &&
        !Match(tag_impl_, html_names::kVideoTag) &&
        !Match(tag_impl_, html_names::kStyleTag))
      tag_impl_ = nullptr;
  }

  enum URLReplacement { kAllowURLReplacement, kDisallowURLReplacement };

  bool GetMatched() const { return matched_; }

  void ProcessAttributes(const HTMLToken::AttributeList& attributes) {
    if (!tag_impl_)
      return;
    for (const HTMLToken::Attribute& html_token_attribute : attributes) {
      AtomicString attribute_name(html_token_attribute.GetName());
      String attribute_value = html_token_attribute.Value();
      ProcessAttribute(attribute_name, attribute_value);
    }
    PostProcessAfterAttributes();
  }

  void PostProcessAfterAttributes() {
    if (Match(tag_impl_, html_names::kImgTag) ||
        (link_is_preload_ && as_attribute_value_ == "image"))
      SetUrlFromImageAttributes();
  }

  void HandlePictureSourceURL(PictureData& picture_data) {
    if (Match(tag_impl_, html_names::kSourceTag) && matched_ &&
        picture_data.source_url.empty()) {
      picture_data.source_url = srcset_image_candidate_.ToString();
      picture_data.source_size_set = source_size_set_;
      picture_data.source_size = source_size_;
      picture_data.picked = true;
    } else if (Match(tag_impl_, html_names::kImgTag) &&
               !picture_data.source_url.empty()) {
      SetUrlToLoad(picture_data.source_url, kAllowURLReplacement);
    }
  }

  std::unique_ptr<PreloadRequest> CreatePreloadRequest(
      const KURL& predicted_base_url,
      const PictureData& picture_data,
      const CachedDocumentParameters& document_parameters,
      const PreloadRequest::ExclusionInfo* exclusion_info,
      bool treat_links_as_in_body) {
    PreloadRequest::RequestType request_type =
        PreloadRequest::kRequestTypePreload;
    absl::optional<ResourceType> type;
    if (ShouldPreconnect()) {
      request_type = PreloadRequest::kRequestTypePreconnect;
    } else {
      if (IsLinkRelPreload()) {
        request_type = PreloadRequest::kRequestTypeLinkRelPreload;
        type = ResourceTypeForLinkPreload();
        if (type == absl::nullopt)
          return nullptr;
      } else if (IsLinkRelModulePreload()) {
        request_type = PreloadRequest::kRequestTypeLinkRelPreload;
        type = ResourceType::kScript;
      }
      if (!ShouldPreload(type)) {
        return nullptr;
      }
    }

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
      // resource_width_ may have originally been set by an explicit width
      // attribute on an img tag but it gets overridden by sizes if present.
      resource_width_ = source_size;
    }

    if (type == absl::nullopt)
      type = GetResourceType();

    // The element's 'referrerpolicy' attribute (if present) takes precedence
    // over the document's referrer policy.
    network::mojom::ReferrerPolicy referrer_policy =
        (referrer_policy_ != network::mojom::ReferrerPolicy::kDefault)
            ? referrer_policy_
            : document_parameters.referrer_policy;
    auto request = PreloadRequest::CreateIfNeeded(
        InitiatorFor(tag_impl_, link_is_modulepreload_), url_to_load_,
        predicted_base_url, type.value(), referrer_policy, is_image_set,
        exclusion_info, resource_width_, resource_height_, request_type);
    if (!request)
      return nullptr;

    bool is_module = (type_attribute_value_ == script_type_names::kModule);
    bool is_script = Match(tag_impl_, html_names::kScriptTag);
    bool is_img = Match(tag_impl_, html_names::kImgTag);
    if ((is_script && is_module) || IsLinkRelModulePreload()) {
      is_module = true;
      request->SetScriptType(mojom::blink::ScriptType::kModule);
    }

    request->SetCrossOrigin(cross_origin_);
    request->SetFetchPriorityHint(fetch_priority_hint_);
    request->SetNonce(nonce_);
    request->SetCharset(Charset());
    request->SetDefer(defer_);

    RenderBlockingBehavior render_blocking_behavior =
        RenderBlockingBehavior::kUnset;
    if (request_type == PreloadRequest::kRequestTypeLinkRelPreload) {
      render_blocking_behavior = RenderBlockingBehavior::kNonBlocking;
    } else if (is_script &&
               (is_module || defer_ == FetchParameters::kLazyLoad)) {
      render_blocking_behavior =
          BlockingAttribute::HasRenderToken(blocking_attribute_value_)
              ? RenderBlockingBehavior::kBlocking
              : (is_async_ ? RenderBlockingBehavior::kPotentiallyBlocking
                           : RenderBlockingBehavior::kNonBlocking);
    } else if (is_script || type == ResourceType::kCSSStyleSheet) {
      // CSS here is render blocking, as non blocking doesn't get preloaded.
      // JS here is a blocking one, as others would've been caught by the
      // previous condition.
      render_blocking_behavior =
          treat_links_as_in_body ? RenderBlockingBehavior::kInBodyParserBlocking
                                 : RenderBlockingBehavior::kBlocking;
    }
    request->SetRenderBlockingBehavior(render_blocking_behavior);

    if (type == ResourceType::kImage && is_img &&
        IsLazyLoadImageDeferable(document_parameters)) {
      return nullptr;
    }
    // Do not set integrity metadata for <link> elements for destinations not
    // supporting SRI (crbug.com/1058045).
    // A corresponding check for non-preload-scanner code path is in
    // PreloadHelper::PreloadIfNeeded().
    // TODO(crbug.com/981419): Honor the integrity attribute value for all
    // supported preload destinations, not just the destinations that support
    // SRI in the first place.
    if (type == ResourceType::kScript || type == ResourceType::kCSSStyleSheet ||
        type == ResourceType::kFont) {
      request->SetIntegrityMetadata(integrity_metadata_);
    }

    if (scanner_type_ == ScannerType::kInsertion)
      request->SetFromInsertionScanner(true);

    if (attributionsrc_attr_set_) {
      DCHECK(is_script || is_img);
      request->SetAttributionReportingEligibleImgOrScript(true);
    }

    if (shared_storage_writable_) {
      DCHECK(is_img);
      request->SetSharedStorageWritable(true);
    }

    return request;
  }

 private:
  void ProcessScriptAttribute(const AtomicString& attribute_name,
                              const String& attribute_value) {
    // FIXME - Don't set crossorigin multiple times.
    if (Match(attribute_name, html_names::kSrcAttr)) {
      SetUrlToLoad(attribute_value, kDisallowURLReplacement);
    } else if (Match(attribute_name, html_names::kCrossoriginAttr)) {
      SetCrossOrigin(attribute_value);
    } else if (Match(attribute_name, html_names::kNonceAttr)) {
      SetNonce(attribute_value);
    } else if (Match(attribute_name, html_names::kAsyncAttr)) {
      is_async_ = true;
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
    } else if (!fetch_priority_hint_set_ &&
               Match(attribute_name, html_names::kFetchpriorityAttr)) {
      SetFetchPriorityHint(attribute_value);
    } else if (Match(attribute_name, html_names::kBlockingAttr)) {
      blocking_attribute_value_ = attribute_value;
    } else if (Match(attribute_name, html_names::kAttributionsrcAttr)) {
      attributionsrc_attr_set_ = true;
    }
  }

  void ProcessImgAttribute(const AtomicString& attribute_name,
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
    } else if (!fetch_priority_hint_set_ &&
               Match(attribute_name, html_names::kFetchpriorityAttr)) {
      SetFetchPriorityHint(attribute_value);
    } else if (Match(attribute_name, html_names::kWidthAttr)) {
      HTMLDimension dimension;
      if (ParseDimensionValue(attribute_value, dimension) &&
          dimension.IsAbsolute()) {
        resource_width_ = dimension.Value();
      }
    } else if (Match(attribute_name, html_names::kHeightAttr)) {
      HTMLDimension dimension;
      if (ParseDimensionValue(attribute_value, dimension) &&
          dimension.IsAbsolute()) {
        resource_height_ = dimension.Value();
      }
    } else if (loading_attr_value_ == LoadingAttributeValue::kAuto &&
               Match(attribute_name, html_names::kLoadingAttr)) {
      loading_attr_value_ = GetLoadingAttributeValue(attribute_value);
    } else if (Match(attribute_name, html_names::kAttributionsrcAttr)) {
      attributionsrc_attr_set_ = true;
    } else if (Match(attribute_name, html_names::kSharedstoragewritableAttr)) {
      shared_storage_writable_ = true;
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

  void ProcessStyleAttribute(const AtomicString& attribute_name,
                             const String& attribute_value) {
    if (Match(attribute_name, html_names::kMediaAttr)) {
      matched_ &= MediaAttributeMatches(*media_values_, attribute_value);
    }
    // No need to parse the `blocking` attribute. Parser-created style elements
    // are implicitly render-blocking as long as the media attribute matches.
  }

  void ProcessLinkAttribute(const AtomicString& attribute_name,
                            const String& attribute_value) {
    // FIXME - Don't set rel/media/crossorigin multiple times.
    if (Match(attribute_name, html_names::kHrefAttr)) {
      SetUrlToLoad(attribute_value, kDisallowURLReplacement);
      // Used in SetUrlFromImageAttributes() when as=image.
      img_src_url_ = attribute_value;
    } else if (Match(attribute_name, html_names::kRelAttr)) {
      LinkRelAttribute rel(attribute_value);
      link_is_style_sheet_ =
          rel.IsStyleSheet() && !rel.IsAlternate() &&
          rel.GetIconType() == mojom::blink::FaviconIconType::kInvalid &&
          !rel.IsDNSPrefetch();
      link_is_preconnect_ = rel.IsPreconnect();
      link_is_preload_ = rel.IsLinkPreload();
      link_is_modulepreload_ = rel.IsModulePreload();
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
    } else if (!fetch_priority_hint_set_ &&
               Match(attribute_name, html_names::kFetchpriorityAttr)) {
      SetFetchPriorityHint(attribute_value);
    } else if (Match(attribute_name, html_names::kBlockingAttr)) {
      blocking_attribute_value_ = attribute_value;
    }
  }

  void ProcessInputAttribute(const AtomicString& attribute_name,
                             const String& attribute_value) {
    // FIXME - Don't set type multiple times.
    if (Match(attribute_name, html_names::kSrcAttr)) {
      SetUrlToLoad(attribute_value, kDisallowURLReplacement);
    } else if (Match(attribute_name, html_names::kTypeAttr)) {
      input_is_image_ =
          EqualIgnoringASCIICase(attribute_value, input_type_names::kImage);
    }
  }

  void ProcessSourceAttribute(const AtomicString& attribute_name,
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
      matched_ &= HTMLImageElement::SupportedImageType(attribute_value,
                                                       disabled_image_types_);
    }
  }

  void ProcessVideoAttribute(const AtomicString& attribute_name,
                             const String& attribute_value) {
    if (Match(attribute_name, html_names::kPosterAttr))
      SetUrlToLoad(attribute_value, kDisallowURLReplacement);
    else if (Match(attribute_name, html_names::kCrossoriginAttr))
      SetCrossOrigin(attribute_value);
  }

  void ProcessAttribute(const AtomicString& attribute_name,
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
    else if (Match(tag_impl_, html_names::kStyleTag))
      ProcessStyleAttribute(attribute_name, attribute_value);
  }

  bool IsLazyLoadImageDeferable(
      const CachedDocumentParameters& document_parameters) {
    if (document_parameters.lazy_load_image_setting ==
        LocalFrame::LazyLoadImageSetting::kDisabled) {
      return false;
    }

    return loading_attr_value_ == LoadingAttributeValue::kLazy;
  }

  void SetUrlToLoad(const String& value, URLReplacement replacement) {
    // We only respect the first src/href, per HTML5:
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/tokenization.html#attribute-name-state
    if (replacement == kDisallowURLReplacement && !url_to_load_.empty())
      return;
    String url = StripLeadingAndTrailingHTMLSpaces(value);
    if (url.empty())
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

  absl::optional<ResourceType> ResourceTypeForLinkPreload() const {
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
    NOTREACHED();
    return ResourceType::kRaw;
  }

  bool ShouldPreconnect() const {
    return Match(tag_impl_, html_names::kLinkTag) && link_is_preconnect_ &&
           !url_to_load_.empty();
  }

  bool IsLinkRelPreload() const {
    return Match(tag_impl_, html_names::kLinkTag) && link_is_preload_ &&
           !url_to_load_.empty();
  }

  bool IsLinkRelModulePreload() const {
    return Match(tag_impl_, html_names::kLinkTag) && link_is_modulepreload_ &&
           !url_to_load_.empty();
  }

  bool ShouldPreloadLink(absl::optional<ResourceType>& type) const {
    if (link_is_style_sheet_) {
      return type_attribute_value_.empty() ||
             MIMETypeRegistry::IsSupportedStyleSheetMIMEType(
                 ContentType(type_attribute_value_).GetType());
    } else if (link_is_preload_) {
      if (type == ResourceType::kImage) {
        return HTMLImageElement::SupportedImageType(type_attribute_value_,
                                                    disabled_image_types_);
      }
      if (type_attribute_value_.empty())
        return true;
      String type_from_attribute = ContentType(type_attribute_value_).GetType();
      if ((type == ResourceType::kFont &&
           !MIMETypeRegistry::IsSupportedFontMIMEType(type_from_attribute)) ||
          (type == ResourceType::kCSSStyleSheet &&
           !MIMETypeRegistry::IsSupportedStyleSheetMIMEType(
               type_from_attribute))) {
        return false;
      }
      return true;
    } else if (link_is_modulepreload_) {
      return true;
    }
    return false;
  }

  bool ShouldPreload(absl::optional<ResourceType>& type) const {
    if (url_to_load_.empty())
      return false;
    if (!matched_)
      return false;
    if (Match(tag_impl_, html_names::kLinkTag))
      return ShouldPreloadLink(type);
    if (Match(tag_impl_, html_names::kInputTag) && !input_is_image_)
      return false;
    if (Match(tag_impl_, html_names::kScriptTag)) {
      ScriptLoader::ScriptTypeAtPrepare script_type =
          ScriptLoader::GetScriptTypeAtPrepare(type_attribute_value_,
                                               language_attribute_value_);
      switch (script_type) {
        case ScriptLoader::ScriptTypeAtPrepare::kInvalid:
          return false;

        case ScriptLoader::ScriptTypeAtPrepare::kImportMap:
          // TODO(crbug.com/922212): External import maps are not yet supported.
          return false;

        case ScriptLoader::ScriptTypeAtPrepare::kSpeculationRules:
          // TODO(crbug.com/1182803): External speculation rules are not yet
          // supported.
          return false;

        case ScriptLoader::ScriptTypeAtPrepare::kWebBundle:
          // External webbundle is not yet supported.
          return false;

        case ScriptLoader::ScriptTypeAtPrepare::kClassic:
        case ScriptLoader::ScriptTypeAtPrepare::kModule:
          if (ScriptLoader::BlockForNoModule(script_type,
                                             nomodule_attribute_value_)) {
            return false;
          }
      }
    }
    return true;
  }

  void ParseSourceSize(const String& attribute_value) {
    source_size_ =
        SizesAttributeParser(media_values_, attribute_value, nullptr).length();
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

  void SetFetchPriorityHint(const String& fetch_priority_hint) {
    fetch_priority_hint_set_ = true;
    fetch_priority_hint_ = GetFetchPriorityAttributeValue(fetch_priority_hint);
  }

  void SetNonce(const String& nonce) { nonce_ = nonce; }

  void SetDefer(FetchParameters::DeferOption defer) { defer_ = defer; }

  bool Defer() const { return defer_ != FetchParameters::kNoDefer; }

  const StringImpl* tag_impl_;
  String url_to_load_;
  ImageCandidate srcset_image_candidate_;
  String charset_;
  bool link_is_style_sheet_ = false;
  bool link_is_preconnect_ = false;
  bool link_is_preload_ = false;
  bool link_is_modulepreload_ = false;
  bool matched_ = true;
  bool input_is_image_ = false;
  String img_src_url_;
  String srcset_attribute_value_;
  String as_attribute_value_;
  String type_attribute_value_;
  String language_attribute_value_;
  String blocking_attribute_value_;
  AtomicString scopes_attribute_value_;
  AtomicString resources_attribute_value_;
  bool nomodule_attribute_value_ = false;
  float source_size_ = 0;
  bool source_size_set_ = false;
  FetchParameters::DeferOption defer_ = FetchParameters::kNoDefer;
  CrossOriginAttributeValue cross_origin_ = kCrossOriginAttributeNotSet;
  mojom::blink::FetchPriorityHint fetch_priority_hint_ =
      mojom::blink::FetchPriorityHint::kAuto;
  bool fetch_priority_hint_set_ = false;
  String nonce_;
  MediaValuesCached* media_values_;
  bool referrer_policy_set_ = false;
  network::mojom::ReferrerPolicy referrer_policy_ =
      network::mojom::ReferrerPolicy::kDefault;
  bool integrity_attr_set_ = false;
  bool is_async_ = false;
  IntegrityMetadataSet integrity_metadata_;
  SubresourceIntegrity::IntegrityFeatures integrity_features_;
  LoadingAttributeValue loading_attr_value_ = LoadingAttributeValue::kAuto;
  TokenPreloadScanner::ScannerType scanner_type_;
  // For explanation, see TokenPreloadScanner's declaration.
  const HashSet<String>* disabled_image_types_;
  bool attributionsrc_attr_set_ = false;
  bool shared_storage_writable_ = false;
  absl::optional<float> resource_width_;
  absl::optional<float> resource_height_;
};

TokenPreloadScanner::TokenPreloadScanner(
    const KURL& document_url,
    std::unique_ptr<CachedDocumentParameters> document_parameters,
    const MediaValuesCached::MediaValuesCachedData& media_values_cached_data,
    const ScannerType scanner_type,
    Vector<ElementLocator> locators)
    : document_url_(document_url),
      in_style_(false),
      in_picture_(false),
      in_script_(false),
      in_script_web_bundle_(false),
      seen_body_(false),
      seen_img_(false),
      template_count_(0),
      document_parameters_(std::move(document_parameters)),
      media_values_(
          MakeGarbageCollected<MediaValuesCached>(media_values_cached_data)),
      scanner_type_(scanner_type),
      lcp_element_matcher_(std::move(locators)) {
  DCHECK(document_parameters_.get());
  DCHECK(media_values_.Get());
  DCHECK(document_url.IsValid());
  css_scanner_.SetReferrerPolicy(document_parameters_->referrer_policy);
}

TokenPreloadScanner::~TokenPreloadScanner() = default;

static void HandleMetaViewport(
    const String& attribute_value,
    const CachedDocumentParameters* document_parameters,
    MediaValuesCached* media_values,
    absl::optional<ViewportDescription>* viewport) {
  if (!document_parameters->viewport_meta_enabled)
    return;
  ViewportDescription description(ViewportDescription::kViewportMeta);
  HTMLMetaElement::GetViewportDescriptionFromContentAttribute(
      attribute_value, description, nullptr,
      document_parameters->viewport_meta_zero_values_quirk);
  if (viewport)
    *viewport = description;
  gfx::SizeF initial_viewport(media_values->DeviceWidth(),
                              media_values->DeviceHeight());
  PageScaleConstraints constraints = description.Resolve(
      initial_viewport, document_parameters->default_viewport_min_width);
  media_values->OverrideViewportDimensions(constraints.layout_size.width(),
                                           constraints.layout_size.height());
}

static void HandleMetaReferrer(const String& attribute_value,
                               CachedDocumentParameters* document_parameters,
                               CSSPreloadScanner* css_scanner) {
  network::mojom::ReferrerPolicy meta_referrer_policy =
      network::mojom::ReferrerPolicy::kDefault;
  if (!attribute_value.empty() && !attribute_value.IsNull() &&
      SecurityPolicy::ReferrerPolicyFromString(
          attribute_value, kSupportReferrerPolicyLegacyKeywords,
          &meta_referrer_policy)) {
    document_parameters->referrer_policy = meta_referrer_policy;
  }
  css_scanner->SetReferrerPolicy(document_parameters->referrer_policy);
}

void TokenPreloadScanner::HandleMetaNameAttribute(
    const HTMLToken& token,
    MetaCHValues& meta_ch_values,
    absl::optional<ViewportDescription>* viewport) {
  const HTMLToken::Attribute* name_attribute =
      token.GetAttributeItem(html_names::kNameAttr);
  if (!name_attribute)
    return;

  String name_attribute_value(name_attribute->Value());
  const HTMLToken::Attribute* content_attribute =
      token.GetAttributeItem(html_names::kContentAttr);
  if (!content_attribute)
    return;

  String content_attribute_value(content_attribute->Value());
  if (EqualIgnoringASCIICase(name_attribute_value, "viewport")) {
    HandleMetaViewport(content_attribute_value, document_parameters_.get(),
                       media_values_.Get(), viewport);
    return;
  }

  if (EqualIgnoringASCIICase(name_attribute_value, "referrer")) {
    HandleMetaReferrer(content_attribute_value, document_parameters_.get(),
                       &css_scanner_);
  }
}

void TokenPreloadScanner::Scan(const HTMLToken& token,
                               const SegmentedString& source,
                               PreloadRequestStream& requests,
                               MetaCHValues& meta_ch_values,
                               absl::optional<ViewportDescription>* viewport,
                               bool* is_csp_meta_tag) {
  if (!document_parameters_->do_html_preload_scanning)
    return;

  switch (token.GetType()) {
    case HTMLToken::kCharacter: {
      if (in_style_) {
        css_scanner_.Scan(token.Data(), source, requests,
                          predicted_base_element_url_, exclusion_info_.get());
      }
      if (in_script_web_bundle_) {
        ScanScriptWebBundle(token.Data(),
                            predicted_base_element_url_.IsEmpty()
                                ? document_url_
                                : predicted_base_element_url_,
                            exclusion_info_);
      }
      return;
    }
    case HTMLToken::kEndTag: {
      const StringImpl* tag_impl = TagImplFor(token.Data());
      lcp_element_matcher_.ObserveEndTag(tag_impl);
      if (Match(tag_impl, html_names::kTemplateTag)) {
        if (template_count_)
          --template_count_;
        return;
      }
      if (template_count_) {
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
        in_script_web_bundle_ = false;
        return;
      }
      if (Match(tag_impl, html_names::kPictureTag)) {
        in_picture_ = false;
        picture_data_.picked = false;
      }
      return;
    }
    case HTMLToken::kStartTag: {
      const StringImpl* tag_impl = TagImplFor(token.Data());
      const bool potentially_lcp_element =
          lcp_element_matcher_.ObserveStartTagAndReportMatch(tag_impl, token);
      if (Match(tag_impl, html_names::kTemplateTag)) {
        bool is_declarative_shadow_root = false;
        const HTMLToken::Attribute* shadowrootmode_attribute =
            token.GetAttributeItem(html_names::kShadowrootmodeAttr);
        if (shadowrootmode_attribute) {
          String shadowrootmode_value(shadowrootmode_attribute->Value());
          is_declarative_shadow_root =
              EqualIgnoringASCIICase(shadowrootmode_value, "open") ||
              EqualIgnoringASCIICase(shadowrootmode_value, "closed");
        }
        // If this is a declarative shadow root <template shadowrootmode>
        // element *and* we're not already inside a non-DSD <template> element,
        // then we leave the template count at zero. Otherwise, increment it.
        if (!(is_declarative_shadow_root && !template_count_)) {
          ++template_count_;
        }
      }
      if (template_count_)
        return;
      // Don't early return, because the StartTagScanner needs to look at these
      // too.
      if (Match(tag_impl, html_names::kStyleTag)) {
        in_style_ = true;
        css_scanner_.SetInBody(seen_img_ || seen_body_);
      }
      if (Match(tag_impl, html_names::kScriptTag)) {
        in_script_ = true;

        const HTMLToken::Attribute* type_attribute =
            token.GetAttributeItem(html_names::kTypeAttr);
        if (type_attribute &&
            ScriptLoader::GetScriptTypeAtPrepare(
                type_attribute->Value(),
                /*language_attribute_value=*/g_empty_atom) ==
                ScriptLoader::ScriptTypeAtPrepare::kWebBundle) {
          in_script_web_bundle_ = true;
        }
      }
      if (Match(tag_impl, html_names::kBaseTag)) {
        // The first <base> element is the one that wins.
        if (!predicted_base_element_url_.IsEmpty())
          return;
        UpdatePredictedBaseURL(token);
        return;
      }
      if (Match(tag_impl, html_names::kMetaTag)) {
        const HTMLToken::Attribute* equiv_attribute =
            token.GetAttributeItem(html_names::kHttpEquivAttr);
        if (equiv_attribute) {
          String equiv_attribute_value(equiv_attribute->Value());
          if (EqualIgnoringASCIICase(equiv_attribute_value,
                                     "content-security-policy")) {
            *is_csp_meta_tag = true;
          } else if (EqualIgnoringASCIICase(equiv_attribute_value,
                                            http_names::kAcceptCH) &&
                     RuntimeEnabledFeatures::
                         ClientHintsMetaHTTPEquivAcceptCHEnabled()) {
            const HTMLToken::Attribute* content_attribute =
                token.GetAttributeItem(html_names::kContentAttr);
            if (content_attribute) {
              meta_ch_values.push_back(
                  MetaCHValue{.value = content_attribute->GetValue(),
                              .type = network::MetaCHType::HttpEquivAcceptCH,
                              .is_doc_preloader =
                                  scanner_type_ == ScannerType::kMainDocument});
            }
          } else if (EqualIgnoringASCIICase(equiv_attribute_value,
                                            http_names::kDelegateCH) &&
                     RuntimeEnabledFeatures::
                         ClientHintsMetaEquivDelegateCHEnabled()) {
            const HTMLToken::Attribute* content_attribute =
                token.GetAttributeItem(html_names::kContentAttr);
            if (content_attribute) {
              meta_ch_values.push_back(
                  MetaCHValue{.value = content_attribute->GetValue(),
                              .type = network::MetaCHType::HttpEquivDelegateCH,
                              .is_doc_preloader =
                                  scanner_type_ == ScannerType::kMainDocument});
            }
          }
          return;
        }

        HandleMetaNameAttribute(token, meta_ch_values, viewport);
      }

      if (Match(tag_impl, html_names::kBodyTag)) {
        seen_body_ = true;
      } else if (Match(tag_impl, html_names::kImgTag)) {
        seen_img_ = true;
      } else if (Match(tag_impl, html_names::kPictureTag)) {
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
          scanner_type_, &document_parameters_->disabled_image_types);
      scanner.ProcessAttributes(token.Attributes());

      if (in_picture_ && media_values_->Width())
        scanner.HandlePictureSourceURL(picture_data_);
      if (in_style_)
        css_scanner_.SetMediaMatches(scanner.GetMatched());
      std::unique_ptr<PreloadRequest> request = scanner.CreatePreloadRequest(
          predicted_base_element_url_, picture_data_, *document_parameters_,
          exclusion_info_.get(), seen_img_ || seen_body_);
      if (request) {
        request->SetInitiatorPosition(
            TextPosition(source.CurrentLine(), source.CurrentColumn()));
        request->SetIsPotentiallyLCPElement(potentially_lcp_element);
        requests.push_back(std::move(request));
      }
      return;
    }
    default: {
      return;
    }
  }
}

void TokenPreloadScanner::UpdatePredictedBaseURL(const HTMLToken& token) {
  DCHECK(predicted_base_element_url_.IsEmpty());
  if (const HTMLToken::Attribute* href_attribute =
          token.GetAttributeItem(html_names::kHrefAttr)) {
    KURL url(document_url_,
             StripLeadingAndTrailingHTMLSpaces(href_attribute->Value()));
    bool is_valid_base_url =
        url.IsValid() && !url.ProtocolIsData() && !url.ProtocolIsJavaScript();
    predicted_base_element_url_ = is_valid_base_url ? url : KURL();
  }
}

// static
std::unique_ptr<HTMLPreloadScanner> HTMLPreloadScanner::Create(
    Document& document,
    HTMLParserOptions options,
    TokenPreloadScanner::ScannerType scanner_type) {
  Vector<ElementLocator> locators;
    if (LocalFrame* frame = document.GetFrame()) {
      if (LCPCriticalPathPredictor* lcpp = frame->GetLCPP()) {
        locators = lcpp->lcp_element_locators();
      }
    }

  return std::make_unique<HTMLPreloadScanner>(
      std::make_unique<HTMLTokenizer>(options), document.Url(),
      std::make_unique<CachedDocumentParameters>(&document),
      MediaValuesCached::MediaValuesCachedData(document), scanner_type,
      /* script_token_scanner=*/nullptr, TakePreloadFn(), locators);
}

// static
HTMLPreloadScanner::BackgroundPtr HTMLPreloadScanner::CreateBackground(
    HTMLDocumentParser* parser,
    HTMLParserOptions options,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    TakePreloadFn take_preload) {
  auto* document = parser->GetDocument();

  Vector<ElementLocator> locators;
    if (LocalFrame* frame = document->GetFrame()) {
      if (LCPCriticalPathPredictor* lcpp = frame->GetLCPP()) {
        locators = lcpp->lcp_element_locators();
      }
    }

  return BackgroundPtr(
      new HTMLPreloadScanner(
          std::make_unique<HTMLTokenizer>(options), document->Url(),
          std::make_unique<CachedDocumentParameters>(document),
          MediaValuesCached::MediaValuesCachedData(*document),
          TokenPreloadScanner::ScannerType::kMainDocument,
          BackgroundHTMLScanner::ScriptTokenScanner::Create(parser),
          std::move(take_preload), locators),
      Deleter{task_runner});
}

HTMLPreloadScanner::HTMLPreloadScanner(
    std::unique_ptr<HTMLTokenizer> tokenizer,
    const KURL& document_url,
    std::unique_ptr<CachedDocumentParameters> document_parameters,
    const MediaValuesCached::MediaValuesCachedData& media_values_cached_data,
    const TokenPreloadScanner::ScannerType scanner_type,
    std::unique_ptr<BackgroundHTMLScanner::ScriptTokenScanner>
        script_token_scanner,
    TakePreloadFn take_preload,
    Vector<ElementLocator> locators)
    : scanner_(document_url,
               std::move(document_parameters),
               media_values_cached_data,
               scanner_type,
               std::move(locators)),
      tokenizer_(std::move(tokenizer)),
      script_token_scanner_(std::move(script_token_scanner)),
      take_preload_(std::move(take_preload)) {}

HTMLPreloadScanner::~HTMLPreloadScanner() = default;

void HTMLPreloadScanner::AppendToEnd(const SegmentedString& source) {
  source_.Append(source);
}

std::unique_ptr<PendingPreloadData> HTMLPreloadScanner::Scan(
    const KURL& starting_base_element_url) {
  auto pending_data = std::make_unique<PendingPreloadData>();

  TRACE_EVENT1("blink", "HTMLPreloadScanner::scan", "source_length",
               source_.length());

  // When we start scanning, our best prediction of the baseElementURL is the
  // real one!
  if (!starting_base_element_url.IsEmpty())
    scanner_.SetPredictedBaseElementURL(starting_base_element_url);

  // The script scanner needs to know whether this is the first script in the
  // chunk being scanned, since it may have different script compile behavior
  // depending on this.
  if (script_token_scanner_)
    script_token_scanner_->set_first_script_in_scan(true);

  while (HTMLToken* token = tokenizer_->NextToken(source_)) {
    if (token->GetType() == HTMLToken::kStartTag)
      tokenizer_->UpdateStateFor(*token);
    bool seen_csp_meta_tag = false;
    scanner_.Scan(*token, source_, pending_data->requests,
                  pending_data->meta_ch_values, &pending_data->viewport,
                  &seen_csp_meta_tag);
    if (script_token_scanner_)
      script_token_scanner_->ScanToken(*token);
    pending_data->has_csp_meta_tag |= seen_csp_meta_tag;
    token->Clear();
    // Don't preload anything if a CSP meta tag is found. We should rarely find
    // them here because the HTMLPreloadScanner is only used for the synchronous
    // parsing path.
    if (seen_csp_meta_tag) {
      // Reset the tokenizer, to avoid re-scanning tokens that we are about to
      // start parsing.
      source_.Clear();
      tokenizer_->Reset();
      return pending_data;
    }
    // Incrementally add preloads when scanning in the background.
    if (take_preload_ && !pending_data->requests.empty()) {
      take_preload_.Run(std::move(pending_data));
      pending_data = std::make_unique<PendingPreloadData>();
    }
  }
  return pending_data;
}

void HTMLPreloadScanner::ScanInBackground(
    const String& source,
    const KURL& document_base_element_url) {
  source_.Append(source);
  take_preload_.Run(Scan(document_base_element_url));
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
  integrity_features =
      SubresourceIntegrityHelper::GetFeatures(document->GetExecutionContext());
  if (document->Loader() && document->Loader()->GetFrame()) {
    lazy_load_image_setting =
        document->Loader()->GetFrame()->GetLazyLoadImageSetting();
  } else {
    lazy_load_image_setting = LocalFrame::LazyLoadImageSetting::kDisabled;
  }
  probe::GetDisabledImageTypes(document->GetExecutionContext(),
                               &disabled_image_types);
}

}  // namespace blink
