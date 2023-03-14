// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/url_info.h"

namespace content {

// We use NavigationRequest::navigation_id_ to provide sandbox id values; this
// function never returns a negative value, so we distinguish unused sandbox ids
// with the following constant.
const int64_t UrlInfo::kInvalidUniqueSandboxId = -1;

UrlInfo::UrlInfo() = default;

UrlInfo::UrlInfo(const UrlInfo& other) = default;

UrlInfo::UrlInfo(const UrlInfoInit& init)
    : url(init.url_),
      origin_isolation_request(init.origin_isolation_request_),
      origin(init.origin_),
      is_sandboxed(init.is_sandboxed_),
      unique_sandbox_id(init.unique_sandbox_id_),
      storage_partition_config(init.storage_partition_config_),
      web_exposed_isolation_info(init.web_exposed_isolation_info_),
      is_pdf(init.is_pdf_),
      common_coop_origin(init.common_coop_origin_) {
  // An origin-keyed process can only be used for origin-keyed agent clusters.
  DCHECK(!requests_origin_keyed_process() || requests_origin_agent_cluster());
  DCHECK(init.is_sandboxed_ ||
         init.unique_sandbox_id_ == kInvalidUniqueSandboxId);
}

UrlInfo::~UrlInfo() = default;

// static
UrlInfo UrlInfo::CreateForTesting(
    const GURL& url_in,
    absl::optional<StoragePartitionConfig> storage_partition_config) {
  return UrlInfo(
      UrlInfoInit(url_in).WithStoragePartitionConfig(storage_partition_config));
}

bool UrlInfo::IsIsolated() const {
  if (!web_exposed_isolation_info)
    return false;
  return web_exposed_isolation_info->is_isolated();
}

UrlInfoInit::UrlInfoInit(UrlInfoInit&) = default;

UrlInfoInit::UrlInfoInit(const GURL& url) : url_(url) {}

UrlInfoInit::UrlInfoInit(const UrlInfo& base)
    : url_(base.url),
      origin_isolation_request_(base.origin_isolation_request),
      origin_(base.origin),
      is_sandboxed_(base.is_sandboxed),
      unique_sandbox_id_(base.unique_sandbox_id),
      storage_partition_config_(base.storage_partition_config),
      web_exposed_isolation_info_(base.web_exposed_isolation_info),
      is_pdf_(base.is_pdf) {}

UrlInfoInit::~UrlInfoInit() = default;

UrlInfoInit& UrlInfoInit::WithOriginIsolationRequest(
    UrlInfo::OriginIsolationRequest origin_isolation_request) {
  origin_isolation_request_ = origin_isolation_request;
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
    absl::optional<StoragePartitionConfig> storage_partition_config) {
  storage_partition_config_ = storage_partition_config;
  return *this;
}

UrlInfoInit& UrlInfoInit::WithWebExposedIsolationInfo(
    absl::optional<WebExposedIsolationInfo> web_exposed_isolation_info) {
  web_exposed_isolation_info_ = web_exposed_isolation_info;
  return *this;
}

UrlInfoInit& UrlInfoInit::WithIsPdf(bool is_pdf) {
  is_pdf_ = is_pdf;
  return *this;
}

UrlInfoInit& UrlInfoInit::WithCommonCoopOrigin(
    const url::Origin& common_coop_origin) {
  common_coop_origin_ = common_coop_origin;
  return *this;
}

}  // namespace content
