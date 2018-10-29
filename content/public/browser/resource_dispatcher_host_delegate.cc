// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/resource_dispatcher_host_delegate.h"

#include "content/public/browser/navigation_data.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/stream_info.h"

namespace content {

ResourceDispatcherHostDelegate::~ResourceDispatcherHostDelegate() {}

void ResourceDispatcherHostDelegate::RequestBeginning(
    net::URLRequest* request,
    ResourceContext* resource_context,
    AppCacheService* appcache_service,
    ResourceType resource_type,
    std::vector<std::unique_ptr<ResourceThrottle>>* throttles) {}

void ResourceDispatcherHostDelegate::DownloadStarting(
    net::URLRequest* request,
    ResourceContext* resource_context,
    bool is_content_initiated,
    bool must_download,
    bool is_new_request,
    std::vector<std::unique_ptr<ResourceThrottle>>* throttles) {}

bool ResourceDispatcherHostDelegate::ShouldInterceptResourceAsStream(
    net::URLRequest* request,
    const std::string& mime_type,
    GURL* origin,
    std::string* payload) {
  return false;
}

void ResourceDispatcherHostDelegate::OnStreamCreated(
    net::URLRequest* request,
    std::unique_ptr<content::StreamInfo> stream) {}

void ResourceDispatcherHostDelegate::OnResponseStarted(
    net::URLRequest* request,
    ResourceContext* resource_context,
    network::ResourceResponse* response) {}

void ResourceDispatcherHostDelegate::OnRequestRedirected(
    const GURL& redirect_url,
    net::URLRequest* request,
    ResourceContext* resource_context,
    network::ResourceResponse* response) {}

void ResourceDispatcherHostDelegate::RequestComplete(
    net::URLRequest* url_request,
    int net_error) {}

// Deprecated.
void ResourceDispatcherHostDelegate::RequestComplete(
    net::URLRequest* url_request) {}

NavigationData* ResourceDispatcherHostDelegate::GetNavigationData(
    net::URLRequest* request) const {
  return nullptr;
}

}  // namespace content
