// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/url_info.h"

#include <sstream>

#include "content/browser/isolation_context.h"

namespace content {

// We use NavigationRequest::navigation_id_ to provide sandbox id values; this
// function never returns a negative value, so we distinguish unused sandbox ids
// with the following constant.
const int64_t UrlInfo::kInvalidUniqueSandboxId = -1;

UrlInfo::UrlInfo() = default;

UrlInfo::UrlInfo(const UrlInfo& other) = default;

UrlInfo::UrlInfo(const UrlInfoInit& init)
    : url(init.url_),
      oac_header_request(init.oac_header_request_),
      is_coop_isolation_requested(init.requests_coop_isolation_),
      is_prefetch_with_cross_site_contamination(
          init.is_prefetch_with_cross_site_contamination_),
      origin(init.origin_),
      is_sandboxed(init.is_sandboxed_),
      unique_sandbox_id(init.unique_sandbox_id_),
      storage_partition_config(init.storage_partition_config_),
      web_exposed_isolation_info(init.web_exposed_isolation_info_),
      is_pdf(init.is_pdf_),
      cross_origin_isolation_key(init.cross_origin_isolation_key_),
      process_selection_user_data(init.process_selection_user_data_) {
  DCHECK(init.is_sandboxed_ ||
         init.unique_sandbox_id_ == kInvalidUniqueSandboxId);
}

UrlInfo::~UrlInfo() = default;

// static
UrlInfo UrlInfo::CreateForTesting(
    const GURL& url_in,
    std::optional<StoragePartitionConfig> storage_partition_config) {
  return UrlInfo(
      UrlInfoInit(url_in).WithStoragePartitionConfig(storage_partition_config));
}

bool UrlInfo::IsIsolated() const {
  bool is_isolated = false;
  if (web_exposed_isolation_info) {
    is_isolated |= web_exposed_isolation_info->is_isolated();
  }

  if (cross_origin_isolation_key) {
    is_isolated |= cross_origin_isolation_key->cross_origin_isolation_mode ==
                   CrossOriginIsolationMode::kConcrete;
  }

  return is_isolated;
}

bool UrlInfo::RequestsOriginKeyedProcess(
    const IsolationContext& context) const {
  // An origin-keyed process should be used if (1) the UrlInfo requires it or
  // (2) the UrlInfo would have used an origin agent cluster based on the lack
  // of header, and the given IsolationContext is in a mode that uses
  // origin-keyed processes by default (i.e., kOriginKeyedProcessesByDefault).
  return (oac_header_request.has_value() &&
          oac_header_request->requires_origin_keyed_process()) ||
         (!oac_header_request.has_value() &&
          context.default_isolation_state().requires_origin_keyed_process());
}

void UrlInfo::WriteIntoTrace(perfetto::TracedProto<TraceProto> proto) const {
  proto->set_url(url.possibly_invalid_spec());
  if (origin.has_value()) {
    proto->set_origin(origin->GetDebugString());
  }
  proto->set_is_sandboxed(is_sandboxed);
  proto->set_is_pdf(is_pdf);
  proto->set_is_coop_isolation_requested(is_coop_isolation_requested);
  int origin_isolation_request = 0;
  if (oac_header_request &&
      oac_header_request->logical_oac_status() ==
          AgentClusterKey::OACStatus::kSiteKeyedByHeader) {
    origin_isolation_request = (1 << 0);
  } else if (oac_header_request &&
             oac_header_request->is_origin_agent_cluster()) {
    origin_isolation_request = (1 << 1);
    if (oac_header_request->requires_origin_keyed_process()) {
      origin_isolation_request += (1 << 2);
    }
  }
  proto->set_origin_isolation_request(origin_isolation_request);
  proto->set_is_prefetch_with_cross_site_contamination(
      is_prefetch_with_cross_site_contamination);
  if (web_exposed_isolation_info.has_value()) {
    proto.Set(TraceProto::kWebExposedIsolationInfo,
              *web_exposed_isolation_info);
  }
  if (storage_partition_config.has_value()) {
    std::stringstream ss;
    ss << *storage_partition_config;
    proto->set_storage_partition_config(ss.str());
  }
}

UrlInfoInit::UrlInfoInit(UrlInfoInit&) = default;

UrlInfoInit::UrlInfoInit(const GURL& url) : url_(url) {}

UrlInfoInit::UrlInfoInit(const UrlInfo& base)
    : url_(base.url),
      oac_header_request_(base.oac_header_request),
      requests_coop_isolation_(base.is_coop_isolation_requested),
      origin_(base.origin),
      is_sandboxed_(base.is_sandboxed),
      unique_sandbox_id_(base.unique_sandbox_id),
      storage_partition_config_(base.storage_partition_config),
      web_exposed_isolation_info_(base.web_exposed_isolation_info),
      is_pdf_(base.is_pdf),
      cross_origin_isolation_key_(base.cross_origin_isolation_key),
      process_selection_user_data_(base.process_selection_user_data) {}

UrlInfoInit::~UrlInfoInit() = default;

UrlInfoInit& UrlInfoInit::WithOACHeaderRequest(
    std::optional<OriginAgentClusterIsolationState> oac_header_request) {
  oac_header_request_ = oac_header_request;
  return *this;
}

UrlInfoInit& UrlInfoInit::WithCOOPSiteIsolation(bool requests_coop_isolation) {
  requests_coop_isolation_ = requests_coop_isolation;
  return *this;
}

UrlInfoInit& UrlInfoInit::WithCrossSitePrefetchContamination(
    bool contaminated) {
  is_prefetch_with_cross_site_contamination_ = contaminated;
  return *this;
}

UrlInfoInit& UrlInfoInit::WithOrigin(const url::Origin& origin) {
  origin_ = origin;
  return *this;
}

UrlInfoInit& UrlInfoInit::WithSandbox(bool is_sandboxed) {
  is_sandboxed_ = is_sandboxed;
  return *this;
}

UrlInfoInit& UrlInfoInit::WithUniqueSandboxId(int unique_sandbox_id) {
  unique_sandbox_id_ = unique_sandbox_id;
  return *this;
}

UrlInfoInit& UrlInfoInit::WithStoragePartitionConfig(
    std::optional<StoragePartitionConfig> storage_partition_config) {
  storage_partition_config_ = storage_partition_config;
  return *this;
}

UrlInfoInit& UrlInfoInit::WithWebExposedIsolationInfo(
    std::optional<WebExposedIsolationInfo> web_exposed_isolation_info) {
  web_exposed_isolation_info_ = web_exposed_isolation_info;
  return *this;
}

UrlInfoInit& UrlInfoInit::WithIsPdf(bool is_pdf) {
  is_pdf_ = is_pdf;
  return *this;
}

UrlInfoInit& UrlInfoInit::WithCrossOriginIsolationKey(
    const std::optional<AgentClusterKey::CrossOriginIsolationKey>&
        cross_origin_isolation_key) {
  cross_origin_isolation_key_ = cross_origin_isolation_key;
  return *this;
}

UrlInfoInit& UrlInfoInit::WithProcessSelectionUserData(
    base::SafeRef<ProcessSelectionUserData> process_selection_user_data) {
  process_selection_user_data_ = std::move(process_selection_user_data);
  return *this;
}

}  // namespace content
