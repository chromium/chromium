// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/web_bundle_handle.h"

#include "content/browser/web_package/web_bundle_handle_tracker.h"
#include "content/browser/web_package/web_bundle_interceptor_for_file.h"
#include "content/browser/web_package/web_bundle_interceptor_for_history_navigation_from_file_or_from_trustable_file.h"
#include "content/browser/web_package/web_bundle_interceptor_for_history_navigation_from_network.h"
#include "content/browser/web_package/web_bundle_interceptor_for_history_navigation_with_existing_reader.h"
#include "content/browser/web_package/web_bundle_interceptor_for_network.h"
#include "content/browser/web_package/web_bundle_interceptor_for_tracked_navigation_from_file.h"
#include "content/browser/web_package/web_bundle_interceptor_for_tracked_navigation_from_trustable_file_or_from_network.h"
#include "content/browser/web_package/web_bundle_interceptor_for_trustable_file.h"
#include "content/browser/web_package/web_bundle_navigation_info.h"
#include "content/browser/web_package/web_bundle_reader.h"
#include "content/browser/web_package/web_bundle_source.h"
#include "content/browser/web_package/web_bundle_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace content {
// static
std::unique_ptr<WebBundleHandle> WebBundleHandle::CreateForFile(
    int frame_tree_node_id) {
  auto handle = base::WrapUnique(new WebBundleHandle());
  handle->SetInterceptor(std::make_unique<WebBundleInterceptorForFile>(
      base::BindOnce(&WebBundleHandle::OnWebBundleFileLoaded,
                     handle->weak_factory_.GetWeakPtr()),
      frame_tree_node_id));
  return handle;
}

// static
std::unique_ptr<WebBundleHandle> WebBundleHandle::CreateForTrustableFile(
    std::unique_ptr<WebBundleSource> source,
    int frame_tree_node_id) {
  DCHECK(source->is_trusted_file());
  auto handle = base::WrapUnique(new WebBundleHandle());
  handle->SetInterceptor(std::make_unique<WebBundleInterceptorForTrustableFile>(
      std::move(source),
      base::BindOnce(&WebBundleHandle::OnWebBundleFileLoaded,
                     handle->weak_factory_.GetWeakPtr()),
      frame_tree_node_id));
  return handle;
}

// static
std::unique_ptr<WebBundleHandle> WebBundleHandle::CreateForNetwork(
    BrowserContext* browser_context,
    int frame_tree_node_id) {
  DCHECK(base::FeatureList::IsEnabled(features::kWebBundlesFromNetwork));
  auto handle = base::WrapUnique(new WebBundleHandle());
  handle->SetInterceptor(std::make_unique<WebBundleInterceptorForNetwork>(
      base::BindOnce(&WebBundleHandle::OnWebBundleFileLoaded,
                     handle->weak_factory_.GetWeakPtr()),
      browser_context, frame_tree_node_id));
  return handle;
}

// static
std::unique_ptr<WebBundleHandle> WebBundleHandle::CreateForTrackedNavigation(
    scoped_refptr<WebBundleReader> reader,
    int frame_tree_node_id) {
  auto handle = base::WrapUnique(new WebBundleHandle());
  switch (reader->source().type()) {
    case WebBundleSource::Type::kTrustedFile:
    case WebBundleSource::Type::kNetwork:
      handle->SetInterceptor(
          std::make_unique<

              WebBundleInterceptorForTrackedNavigationFromTrustableFileOrFromNetwork>(
              std::move(reader),
              base::BindOnce(&WebBundleHandle::OnWebBundleFileLoaded,
                             handle->weak_factory_.GetWeakPtr()),
              frame_tree_node_id));
      break;
    case WebBundleSource::Type::kFile:
      handle->SetInterceptor(std::make_unique<

                             WebBundleInterceptorForTrackedNavigationFromFile>(
          std::move(reader),
          base::BindOnce(&WebBundleHandle::OnWebBundleFileLoaded,
                         handle->weak_factory_.GetWeakPtr()),
          frame_tree_node_id));
      break;
  }
  return handle;
}

// static
std::unique_ptr<WebBundleHandle> WebBundleHandle::MaybeCreateForNavigationInfo(
    std::unique_ptr<WebBundleNavigationInfo> navigation_info,
    int frame_tree_node_id) {
  auto handle = base::WrapUnique(new WebBundleHandle());
  if (navigation_info->GetReader()) {
    scoped_refptr<WebBundleReader> reader = navigation_info->GetReader().get();
    handle->SetInterceptor(
        std::make_unique<

            WebBundleInterceptorForHistoryNavigationWithExistingReader>(
            std::move(reader), navigation_info->target_inner_url(),
            base::BindOnce(&WebBundleHandle::OnWebBundleFileLoaded,
                           handle->weak_factory_.GetWeakPtr()),
            frame_tree_node_id));
  } else if (navigation_info->source().is_network()) {
    handle->SetInterceptor(std::make_unique<

                           WebBundleInterceptorForHistoryNavigationFromNetwork>(
        navigation_info->source().Clone(), navigation_info->target_inner_url(),
        base::BindOnce(&WebBundleHandle::OnWebBundleFileLoaded,
                       handle->weak_factory_.GetWeakPtr()),
        frame_tree_node_id));
  } else {
    DCHECK(navigation_info->source().is_trusted_file() ||
           navigation_info->source().is_file());
    handle->SetInterceptor(
        std::make_unique<

            WebBundleInterceptorForHistoryNavigationFromFileOrFromTrustableFile>(
            navigation_info->source().Clone(),
            navigation_info->target_inner_url(),
            base::BindOnce(&WebBundleHandle::OnWebBundleFileLoaded,
                           handle->weak_factory_.GetWeakPtr()),
            frame_tree_node_id));
  }
  return handle;
}

WebBundleHandle::WebBundleHandle() = default;

WebBundleHandle::~WebBundleHandle() = default;

std::unique_ptr<NavigationLoaderInterceptor>
WebBundleHandle::TakeInterceptor() {
  DCHECK(interceptor_);
  return std::move(interceptor_);
}

void WebBundleHandle::CreateURLLoaderFactory(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
    mojo::Remote<network::mojom::URLLoaderFactory> fallback_factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(url_loader_factory_);

  url_loader_factory_->SetFallbackFactory(std::move(fallback_factory));
  url_loader_factory_->Clone(std::move(receiver));
}

std::unique_ptr<WebBundleHandleTracker> WebBundleHandle::MaybeCreateTracker() {
  if (!url_loader_factory_)
    return nullptr;
  return std::make_unique<WebBundleHandleTracker>(
      url_loader_factory_->reader(), navigation_info_->target_inner_url());
}

bool WebBundleHandle::IsReadyForLoading() {
  return !!url_loader_factory_;
}

void WebBundleHandle::SetInterceptor(
    std::unique_ptr<NavigationLoaderInterceptor> interceptor) {
  interceptor_ = std::move(interceptor);
}

void WebBundleHandle::OnWebBundleFileLoaded(
    const GURL& target_inner_url,
    std::unique_ptr<WebBundleURLLoaderFactory> url_loader_factory) {
  auto source = url_loader_factory->reader()->source().Clone();
  if (source->is_file())
    claimed_url_ = target_inner_url;
  navigation_info_ = std::make_unique<WebBundleNavigationInfo>(
      std::move(source), target_inner_url,
      url_loader_factory->reader()->GetWeakPtr());
  url_loader_factory_ = std::move(url_loader_factory);
}
}  // namespace content
