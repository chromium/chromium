// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_PRELOAD_REQUEST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_PRELOAD_REQUEST_H_

#include <memory>

#include "base/memory/ptr_util.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/script/script_type.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/platform/loader/fetch/client_hints_preferences.h"
#include "third_party/blink/renderer/platform/loader/fetch/cross_origin_attribute_value.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/integrity_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"

namespace blink {

class Document;
class Resource;
enum class ResourceType : uint8_t;

class CORE_EXPORT PreloadRequest {
  USING_FAST_MALLOC(PreloadRequest);

 public:
  class CORE_EXPORT ExclusionInfo : public RefCounted<ExclusionInfo> {
    USING_FAST_MALLOC(ExclusionInfo);

   public:
    ExclusionInfo(const KURL& document_url,
                  HashSet<KURL> scopes,
                  HashSet<KURL> resources);
    virtual ~ExclusionInfo();

    // Disallow copy and assign.
    ExclusionInfo(const ExclusionInfo&) = delete;
    ExclusionInfo& operator=(const ExclusionInfo&) = delete;

    const KURL& document_url() const { return document_url_; }
    const HashSet<KURL>& scopes() const { return scopes_; }
    const HashSet<KURL>& resources() const { return resources_; }

    bool ShouldExclude(const KURL& base_url, const String& resource_url) const;

   private:
    const KURL document_url_;
    const HashSet<KURL> scopes_;
    const HashSet<KURL> resources_;
  };

  enum RequestType {
    kRequestTypePreload,
    kRequestTypePreconnect,
    kRequestTypeLinkRelPreload
  };

  enum ReferrerSource { kDocumentIsReferrer, kBaseUrlIsReferrer };

  static std::unique_ptr<PreloadRequest> CreateIfNeeded(
      const String& initiator_name,
      const TextPosition& initiator_position,
      const String& resource_url,
      const KURL& base_url,
      ResourceType resource_type,
      const network::mojom::ReferrerPolicy referrer_policy,
      ReferrerSource referrer_source,
      ResourceFetcher::IsImageSet is_image_set,
      const ExclusionInfo* exclusion_info,
      const FetchParameters::ResourceWidth& resource_width =
          FetchParameters::ResourceWidth(),
      const ClientHintsPreferences& client_hints_preferences =
          ClientHintsPreferences(),
      RequestType request_type = kRequestTypePreload);

  Resource* Start(Document*);

  void SetDefer(FetchParameters::DeferOption defer) { defer_ = defer; }
  FetchParameters::DeferOption DeferOption() const { return defer_; }

  void SetCharset(const String& charset) { charset_ = charset; }
  void SetCrossOrigin(CrossOriginAttributeValue cross_origin) {
    cross_origin_ = cross_origin;
  }
  CrossOriginAttributeValue CrossOrigin() const { return cross_origin_; }

  void SetImportance(mojom::FetchImportanceMode importance) {
    importance_ = importance;
  }
  mojom::FetchImportanceMode Importance() const { return importance_; }

  void SetNonce(const String& nonce) { nonce_ = nonce; }
  const String& Nonce() const { return nonce_; }

  ResourceType GetResourceType() const { return resource_type_; }

  const String& ResourceURL() const { return resource_url_; }
  float ResourceWidth() const {
    return resource_width_.is_set ? resource_width_.width : 0;
  }
  const KURL& BaseURL() const { return base_url_; }
  bool IsPreconnect() const { return request_type_ == kRequestTypePreconnect; }
  bool IsLinkRelPreload() const {
    return request_type_ == kRequestTypeLinkRelPreload;
  }
  const ClientHintsPreferences& Preferences() const {
    return client_hints_preferences_;
  }
  network::mojom::ReferrerPolicy GetReferrerPolicy() const {
    return referrer_policy_;
  }

  void SetScriptType(mojom::blink::ScriptType script_type) {
    script_type_ = script_type;
  }

  // Only scripts and css stylesheets need to have integrity set on preloads.
  // This is because neither resource keeps raw data around to redo an
  // integrity check. A resource in memory cache needs integrity
  // data cached to match an outgoing request.
  void SetIntegrityMetadata(const IntegrityMetadataSet& metadata_set) {
    integrity_metadata_ = metadata_set;
  }
  const IntegrityMetadataSet& IntegrityMetadataForTestingOnly() const {
    return integrity_metadata_;
  }
  void SetFromInsertionScanner(const bool from_insertion_scanner) {
    from_insertion_scanner_ = from_insertion_scanner;
  }

  bool IsImageSetForTestingOnly() const {
    return is_image_set_ == ResourceFetcher::kImageIsImageSet;
  }

  void SetRenderBlockingBehavior(
      RenderBlockingBehavior render_blocking_behavior) {
    render_blocking_behavior_ = render_blocking_behavior;
  }

 private:
  PreloadRequest(const String& initiator_name,
                 const TextPosition& initiator_position,
                 const String& resource_url,
                 const KURL& base_url,
                 ResourceType resource_type,
                 const FetchParameters::ResourceWidth& resource_width,
                 const ClientHintsPreferences& client_hints_preferences,
                 RequestType request_type,
                 const network::mojom::ReferrerPolicy referrer_policy,
                 ReferrerSource referrer_source,
                 ResourceFetcher::IsImageSet is_image_set)
      : initiator_name_(initiator_name),
        initiator_position_(initiator_position),
        resource_url_(resource_url),
        base_url_(base_url),
        resource_type_(resource_type),
        script_type_(mojom::blink::ScriptType::kClassic),
        cross_origin_(kCrossOriginAttributeNotSet),
        importance_(mojom::FetchImportanceMode::kImportanceAuto),
        defer_(FetchParameters::kNoDefer),
        resource_width_(resource_width),
        client_hints_preferences_(client_hints_preferences),
        request_type_(request_type),
        referrer_policy_(referrer_policy),
        referrer_source_(referrer_source),
        from_insertion_scanner_(false),
        is_image_set_(is_image_set),
        is_lazy_load_image_enabled_(false) {}

  KURL CompleteURL(Document*);

  const String initiator_name_;
  const TextPosition initiator_position_;
  const String resource_url_;
  const KURL base_url_;
  String charset_;
  const ResourceType resource_type_;
  mojom::blink::ScriptType script_type_;
  CrossOriginAttributeValue cross_origin_;
  mojom::FetchImportanceMode importance_;
  String nonce_;
  FetchParameters::DeferOption defer_;
  const FetchParameters::ResourceWidth resource_width_;
  const ClientHintsPreferences client_hints_preferences_;
  const RequestType request_type_;
  const network::mojom::ReferrerPolicy referrer_policy_;
  const ReferrerSource referrer_source_;
  IntegrityMetadataSet integrity_metadata_;
  RenderBlockingBehavior render_blocking_behavior_ =
      RenderBlockingBehavior::kUnset;
  bool from_insertion_scanner_;
  const ResourceFetcher::IsImageSet is_image_set_;
  bool is_lazy_load_image_enabled_;
};

typedef Vector<std::unique_ptr<PreloadRequest>> PreloadRequestStream;

}  // namespace blink

#endif
