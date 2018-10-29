/*
 * Copyright (C) 2012 Google, Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_FETCH_PARAMETERS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_FETCH_PARAMETERS_H_

#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/platform/cross_origin_attribute_value.h"
#include "third_party/blink/renderer/platform/loader/fetch/client_hints_preferences.h"
#include "third_party/blink/renderer/platform/loader/fetch/integrity_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/text_resource_decoder_options.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/noncopyable.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"

namespace blink {

class SecurityOrigin;

// A FetchParameters is a "parameter object" for
// ResourceFetcher::requestResource to avoid the method having too many
// arguments.
//
// This class is thread-bound. Do not copy/pass an instance across threads.
class PLATFORM_EXPORT FetchParameters {
  DISALLOW_NEW();

 public:
  enum DeferOption { kNoDefer, kLazyLoad, kIdleLoad };
  enum class SpeculativePreloadType {
    kNotSpeculative,
    kInDocument,  // The request was discovered in the main document
    kInserted     // The request was discovered in a document.write()
  };
  enum ImageRequestOptimization {
    kNone = 0,          // No optimization.
    kAllowPlaceholder,  // The image is allowed to be a placeholder.
    kDeferImageLoad,  // Defer loading the image from network. Full image might
                      // still load if the request is already-loaded or in
                      // memory cache.
  };
  struct ResourceWidth {
    DISALLOW_NEW();
    float width;
    bool is_set;

    ResourceWidth() : width(0), is_set(false) {}
  };

  explicit FetchParameters(const ResourceRequest&);
  FetchParameters(const ResourceRequest&, const ResourceLoaderOptions&);
  ~FetchParameters();

  ResourceRequest& MutableResourceRequest() { return resource_request_; }
  const ResourceRequest& GetResourceRequest() const {
    return resource_request_;
  }
  const KURL& Url() const { return resource_request_.Url(); }

  void SetRequestContext(mojom::RequestContextType context) {
    resource_request_.SetRequestContext(context);
  }

  void SetFetchImportanceMode(mojom::FetchImportanceMode importance_mode) {
    resource_request_.SetFetchImportanceMode(importance_mode);
  }

  const TextResourceDecoderOptions& DecoderOptions() const {
    return decoder_options_;
  }
  void SetDecoderOptions(const TextResourceDecoderOptions& decoder_options) {
    decoder_options_ = decoder_options;
  }
  void OverrideContentType(
      TextResourceDecoderOptions::ContentType content_type) {
    decoder_options_.OverrideContentType(content_type);
  }
  void SetCharset(const WTF::TextEncoding& charset) {
    SetDecoderOptions(TextResourceDecoderOptions(
        TextResourceDecoderOptions::kPlainTextContent, charset));
  }

  ResourceLoaderOptions& MutableOptions() { return options_; }
  const ResourceLoaderOptions& Options() const { return options_; }

  DeferOption Defer() const { return defer_; }
  void SetDefer(DeferOption defer) { defer_ = defer; }

  ResourceWidth GetResourceWidth() const { return resource_width_; }
  void SetResourceWidth(ResourceWidth);

  ClientHintsPreferences& GetClientHintsPreferences() {
    return client_hint_preferences_;
  }

  bool IsSpeculativePreload() const {
    return speculative_preload_type_ != SpeculativePreloadType::kNotSpeculative;
  }
  SpeculativePreloadType GetSpeculativePreloadType() const {
    return speculative_preload_type_;
  }
  void SetSpeculativePreloadType(SpeculativePreloadType,
                                 double discovery_time = 0);

  bool IsLinkPreload() const { return options_.initiator_info.is_link_preload; }
  void SetLinkPreload(bool is_link_preload) {
    options_.initiator_info.is_link_preload = is_link_preload;
  }

  bool IsStaleRevalidation() const { return is_stale_revalidation_; }
  void SetStaleRevalidation(bool is_stale_revalidation) {
    is_stale_revalidation_ = is_stale_revalidation;
  }

  void SetContentSecurityCheck(
      ContentSecurityPolicyDisposition content_security_policy_option) {
    options_.content_security_policy_option = content_security_policy_option;
  }
  // Configures the request to use the "cors" mode and the credentials mode
  // specified by the crossOrigin attribute.
  void SetCrossOriginAccessControl(const SecurityOrigin*,
                                   CrossOriginAttributeValue);
  // Configures the request to use the "cors" mode and the specified
  // credentials mode.
  void SetCrossOriginAccessControl(const SecurityOrigin*,
                                   network::mojom::FetchCredentialsMode);
  const IntegrityMetadataSet IntegrityMetadata() const {
    return options_.integrity_metadata;
  }
  void SetIntegrityMetadata(const IntegrityMetadataSet& metadata) {
    options_.integrity_metadata = metadata;
  }

  String ContentSecurityPolicyNonce() const {
    return options_.content_security_policy_nonce;
  }
  void SetContentSecurityPolicyNonce(const String& nonce) {
    options_.content_security_policy_nonce = nonce;
  }

  void SetParserDisposition(ParserDisposition parser_disposition) {
    options_.parser_disposition = parser_disposition;
  }

  void SetCacheAwareLoadingEnabled(
      CacheAwareLoadingEnabled cache_aware_loading_enabled) {
    options_.cache_aware_loading_enabled = cache_aware_loading_enabled;
  }

  void MakeSynchronous();

  ImageRequestOptimization GetImageRequestOptimization() const {
    return image_request_optimization_;
  }

  // Configures the request to load an image as a placeholder and sets the
  // Client LoFi preview bit.
  void SetClientLoFiPlaceholder();

  // Configures the request to load an image as a placeholder and sets the
  // lazy image load bit.
  void SetLazyImagePlaceholder();

  // Configures the request to load an image placeholder if the request is
  // eligible (e.g. the url's protocol is HTTP, etc.). If this request is
  // non-eligible, this method doesn't modify the ResourceRequest. Calling this
  // method sets image_request_optimization_ to the appropriate value.
  void SetAllowImagePlaceholder();

 private:
  ResourceRequest resource_request_;
  // |decoder_options_|'s ContentType is set to |kPlainTextContent| in
  // FetchParameters but is later overridden by ResourceFactory::ContentType()
  // in ResourceFetcher::PrepareRequest() before actual use.
  TextResourceDecoderOptions decoder_options_;
  ResourceLoaderOptions options_;
  SpeculativePreloadType speculative_preload_type_;
  DeferOption defer_;
  ResourceWidth resource_width_;
  ClientHintsPreferences client_hint_preferences_;
  ImageRequestOptimization image_request_optimization_;
  bool is_stale_revalidation_ = false;
};

}  // namespace blink

#endif
