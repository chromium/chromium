// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_REQUEST_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_REQUEST_H_

#include <optional>
#include <variant>

#include "base/memory/weak_ptr.h"
#include "content/browser/preloading/prefetch/prefetch_type.h"
#include "content/browser/preloading/speculation_rules/speculation_rules_tags.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "net/http/http_no_vary_search_data.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/origin.h"

namespace content {

class PrefetchDocumentManager;
class RenderFrameHostImpl;

// `PrefetchRendererInitiatorInfo` or `PrefetchBrowserInitiatorInfo` is created
// and attached to `PrefetchRequest` for a renderer-initiated or
// browser-initiated prefetch, respectively.
//
// They represents the initiator information and other request parameters
// specific to renderer-initiated or browser-initiated prefetches.

// For renderer-initiated prefetches: the initiator Document and its
// `RenderFrameHost`, `PrefetchDocumentManager`, etc.
class CONTENT_EXPORT PrefetchRendererInitiatorInfo final {
 public:
  PrefetchRendererInitiatorInfo(
      RenderFrameHostImpl& render_frame_host,
      base::WeakPtr<PrefetchDocumentManager> prefetch_document_manager);
  ~PrefetchRendererInitiatorInfo();

  // Move-only.
  PrefetchRendererInitiatorInfo(const PrefetchRendererInitiatorInfo&) = delete;
  PrefetchRendererInitiatorInfo& operator=(
      const PrefetchRendererInitiatorInfo&) = delete;
  PrefetchRendererInitiatorInfo(PrefetchRendererInitiatorInfo&&);

  GlobalRenderFrameHostId GetRenderFrameHostId() const {
    return render_frame_host_id_;
  }
  RenderFrameHostImpl* GetRenderFrameHost() const;
  PrefetchDocumentManager* prefetch_document_manager() const {
    return prefetch_document_manager_.get();
  }
  const std::optional<base::UnguessableToken>& devtools_navigation_token()
      const {
    return devtools_navigation_token_;
  }
  ukm::SourceId ukm_source_id() const { return ukm_source_id_; }
  size_t url_hash() const { return url_hash_; }

 private:
  // The RenderFrameHostId/PrefetchDocumentManager of the Document that
  // triggered the prefetch.
  GlobalRenderFrameHostId render_frame_host_id_;

  base::WeakPtr<PrefetchDocumentManager> prefetch_document_manager_;

  // A DevTools token used to identify initiator document.
  std::optional<base::UnguessableToken> devtools_navigation_token_;

  ukm::SourceId ukm_source_id_;

  // Used by metrics for equality checks.
  size_t url_hash_;
};

// For browser-initiated prefetches.
class CONTENT_EXPORT PrefetchBrowserInitiatorInfo final {
 public:
  explicit PrefetchBrowserInitiatorInfo(
      const std::string& embedder_histogram_suffix);
  ~PrefetchBrowserInitiatorInfo();

  // Move-only.
  PrefetchBrowserInitiatorInfo(const PrefetchBrowserInitiatorInfo&) = delete;
  PrefetchBrowserInitiatorInfo& operator=(const PrefetchBrowserInitiatorInfo&) =
      delete;
  PrefetchBrowserInitiatorInfo(PrefetchBrowserInitiatorInfo&&);

  const std::string& embedder_histogram_suffix() const {
    return embedder_histogram_suffix_;
  }

 private:
  // The suffix string of embedder triggers used for generating histogram
  // recorded per trigger.
  std::string embedder_histogram_suffix_;
};

// `PrefetchRequest` represents request parameters to `PrefetchService` to
// prefetch a URL.
//
// TODO(https://crbug.com/437631382): Incrementally migrate the
// `PrefetchContainer` constructor arguments into `PrefetchRequest`, so that
// eventually all parameters are passed via `PrefetchRequest` from the current
// callers of the `PrefetchContainer` constructors. After that, we can make the
// callers to construct `PrefetchRequest` instead of `PrefetchContainer`.
class CONTENT_EXPORT PrefetchRequest final {
 public:
  PrefetchRequest(
      const PrefetchType& prefetch_type,
      const std::optional<net::HttpNoVarySearchData> no_vary_search_hint,
      const std::optional<url::Origin>& referring_origin,
      std::optional<SpeculationRulesTags> speculation_rules_tags,
      std::variant<PrefetchRendererInitiatorInfo, PrefetchBrowserInitiatorInfo>
          info);
  ~PrefetchRequest();

  const PrefetchType& prefetch_type() const { return prefetch_type_; }
  const std::optional<net::HttpNoVarySearchData>& no_vary_search_hint() const {
    return no_vary_search_hint_;
  }
  const std::optional<url::Origin>& referring_origin() const {
    return referring_origin_;
  }

  const std::optional<SpeculationRulesTags>& speculation_rules_tags() const {
    return speculation_rules_tags_;
  }

  // Returns non-null if renderer-initiated/browser-initiated, respectively.
  // Exactly one of them returns non-null.
  const PrefetchRendererInitiatorInfo* GetRendererInitiatorInfo() const;
  const PrefetchBrowserInitiatorInfo* GetBrowserInitiatorInfo() const;

 private:
  // The type of this prefetch. This controls some specific details about how
  // the prefetch is handled, including whether an isolated network context or
  // the default network context is used to perform the prefetch, whether or
  // not the preftch proxy is used, and whether or not subresources are
  // prefetched.
  const PrefetchType prefetch_type_;

  // The No-Vary-Search hint of the prefetch, which is specified by the
  // speculation rules and can be different from actual
  // `PrefetchContainer::no_vary_search_data_`.
  const std::optional<net::HttpNoVarySearchData> no_vary_search_hint_;

  // The origin and URL that initiates the prefetch request.
  // For renderer-initiated prefetch, this is calculated by referring
  // RenderFrameHost's LastCommittedOrigin. For browser-initiated prefetch, this
  // is sometimes explicitly passed via ctor.
  const std::optional<url::Origin> referring_origin_;

  // -------- Parameters that can have non-default values only for
  // -------- renderer-initiated prefetches:

  // The tags of the speculation rules that triggered this prefetch, and this
  // field is non-null if and only if this is created by SpeculationRules
  // prefech. These are assumed to have been validated by the time this is
  // constructed.
  std::optional<SpeculationRulesTags> speculation_rules_tags_;

  const std::variant<PrefetchRendererInitiatorInfo,
                     PrefetchBrowserInitiatorInfo>
      initiator_info_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_REQUEST_H_
