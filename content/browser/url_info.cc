// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/url_info.h"

namespace content {

UrlInfo::UrlInfo(const UrlInfo& other) = default;

UrlInfo::UrlInfo()
    : web_exposed_isolation_info(WebExposedIsolationInfo::CreateNonIsolated()) {
}

UrlInfo::UrlInfo(const UrlInfoInit& init)
    : url(init.url_),
      origin_isolation_request(init.origin_isolation_request_),
      origin(init.origin_),
      storage_partition_config(init.storage_partition_config_),
      web_exposed_isolation_info(init.web_exposed_isolation_info_),
      is_pdf(init.is_pdf_) {
  // An origin-keyed process can only be used for origin-keyed agent clusters.
  DCHECK(!requests_origin_keyed_process() || requests_origin_agent_cluster());
}

UrlInfo::~UrlInfo() = default;

// static
UrlInfo UrlInfo::CreateForTesting(
    const GURL& url_in,
    absl::optional<StoragePartitionConfig> storage_partition_config) {
  return UrlInfo(UrlInfoInit(url_in)
                     .WithOrigin(url::Origin::Create(url_in))
                     .WithStoragePartitionConfig(storage_partition_config));
}

UrlInfoInit::UrlInfoInit(UrlInfoInit&) = default;

UrlInfoInit::UrlInfoInit(const GURL& url)
    : url_(url),
      origin_(url::Origin::Create(url)),
      web_exposed_isolation_info_(
          WebExposedIsolationInfo::CreateNonIsolated()) {}

UrlInfoInit::UrlInfoInit(const UrlInfo& base)
    : url_(base.url),
      origin_isolation_request_(base.origin_isolation_request),
      origin_(base.origin),
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

UrlInfoInit& UrlInfoInit::WithStoragePartitionConfig(
    absl::optional<StoragePartitionConfig> storage_partition_config) {
  storage_partition_config_ = storage_partition_config;
  return *this;
}

UrlInfoInit& UrlInfoInit::WithWebExposedIsolationInfo(
    const WebExposedIsolationInfo& web_exposed_isolation_info) {
  web_exposed_isolation_info_ = web_exposed_isolation_info;
  return *this;
}

UrlInfoInit& UrlInfoInit::WithIsPdf(bool is_pdf) {
  is_pdf_ = is_pdf;
  return *this;
}

}  // namespace content
