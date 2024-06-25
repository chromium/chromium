// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/resource_load_info_notifier_wrapper.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "net/base/ip_endpoint.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/loader/network_utils.h"
#include "third_party/blink/public/common/loader/record_load_histograms.h"
#include "third_party/blink/public/common/loader/resource_type_util.h"
#include "third_party/blink/public/mojom/loader/resource_load_info_notifier.mojom.h"
#include "third_party/blink/public/platform/weak_wrapper_resource_load_info_notifier.h"

namespace blink {

ResourceLoadInfoNotifierWrapper::ResourceLoadInfoNotifierWrapper(
    base::WeakPtr<WeakWrapperResourceLoadInfoNotifier>
        weak_wrapper_resource_load_info_notifier)
    : ResourceLoadInfoNotifierWrapper(
          std::move(weak_wrapper_resource_load_info_notifier),
          base::SingleThreadTaskRunner::GetCurrentDefault()) {}

ResourceLoadInfoNotifierWrapper::ResourceLoadInfoNotifierWrapper(
    base::WeakPtr<WeakWrapperResourceLoadInfoNotifier>
        weak_wrapper_resource_load_info_notifier,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : weak_wrapper_resource_load_info_notifier_(
          std::move(weak_wrapper_resource_load_info_notifier)),
      task_runner_(std::move(task_runner)) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ResourceLoadInfoNotifierWrapper::~ResourceLoadInfoNotifierWrapper() = default;

#if BUILDFLAG(IS_ANDROID)
void ResourceLoadInfoNotifierWrapper::NotifyUpdateUserGestureCarryoverInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (task_runner_->BelongsToCurrentThread()) {
    if (weak_wrapper_resource_load_info_notifier_) {
      weak_wrapper_resource_load_info_notifier_
          ->NotifyUpdateUserGestureCarryoverInfo();
    }
    return;
  }
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&mojom::ResourceLoadInfoNotifier::
                                    NotifyUpdateUserGestureCarryoverInfo,
                                weak_wrapper_resource_load_info_notifier_));
}
#endif

void ResourceLoadInfoNotifierWrapper::NotifyResourceLoadInitiated(
    int64_t request_id,
    const GURL& request_url,
    const std::string& http_method,
    const GURL& referrer,
    network::mojom::RequestDestination request_destination,
    net::RequestPriority request_priority,
    bool is_ad_resource) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(!resource_load_info_);
  resource_load_info_ = mojom::ResourceLoadInfo::New();
  resource_load_info_->method = http_method;
  resource_load_info_->original_url = request_url;
  resource_load_info_->final_url = request_url;
  resource_load_info_->request_destination = request_destination;
  resource_load_info_->request_id = request_id;
  resource_load_info_->referrer = referrer;
  resource_load_info_->network_info = mojom::CommonNetworkInfo::New();
  resource_load_info_->request_priority = request_priority;
  is_ad_resource_ = is_ad_resource;
}

void ResourceLoadInfoNotifierWrapper::NotifyResourceRedirectReceived(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr redirect_response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(resource_load_info_);
  resource_load_info_->final_url = redirect_info.new_url;
  resource_load_info_->method = redirect_info.new_method;
  resource_load_info_->referrer = GURL(redirect_info.new_referrer);
  mojom::RedirectInfoPtr net_redirect_info = mojom::RedirectInfo::New();
  net_redirect_info->origin_of_new_url =
      url::Origin::Create(redirect_info.new_url);
  net_redirect_info->network_info = mojom::CommonNetworkInfo::New();
  net_redirect_info->network_info->network_accessed =
      redirect_response->network_accessed;
  net_redirect_info->network_info->always_access_network =
      network_utils::AlwaysAccessNetwork(redirect_response->headers);
  net_redirect_info->network_info->remote_endpoint =
      redirect_response->remote_endpoint;
  resource_load_info_->redirect_info_chain.push_back(
      std::move(net_redirect_info));
}

void ResourceLoadInfoNotifierWrapper::NotifyResourceResponseReceived(
    network::mojom::URLResponseHeadPtr response_head) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (response_head->network_accessed) {
    if (resource_load_info_->request_destination ==
        network::mojom::RequestDestination::kDocument) {
      UMA_HISTOGRAM_ENUMERATION("Net.ConnectionInfo.MainFrame",
                                response_head->connection_info);
    } else {
      UMA_HISTOGRAM_ENUMERATION("Net.ConnectionInfo.SubResource",
                                response_head->connection_info);
    }
  }

  resource_load_info_->mime_type = response_head->mime_type;
  resource_load_info_->load_timing_info = response_head->load_timing;
  resource_load_info_->network_info->network_accessed =
      response_head->network_accessed;
  resource_load_info_->network_info->always_access_network =
      network_utils::AlwaysAccessNetwork(response_head->headers);
  resource_load_info_->network_info->remote_endpoint =
      response_head->remote_endpoint;
  if (response_head->headers) {
    resource_load_info_->http_status_code =
        response_head->headers->response_code();
  }

  if (task_runner_->BelongsToCurrentThread()) {
    if (weak_wrapper_resource_load_info_notifier_) {
      weak_wrapper_resource_load_info_notifier_->NotifyResourceResponseReceived(
          resource_load_info_->request_id,
          url::SchemeHostPort(resource_load_info_->final_url),
          std::move(response_head), resource_load_info_->request_destination,
          is_ad_resource_);
    }
    return;
  }

  // Make a deep copy of URLResponseHead before passing it cross-thread.
  if (response_head->headers) {
    response_head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        response_head->headers->raw_headers());
  }
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &mojom::ResourceLoadInfoNotifier::NotifyResourceResponseReceived,
          weak_wrapper_resource_load_info_notifier_,
          resource_load_info_->request_id,
          url::SchemeHostPort(resource_load_info_->final_url),
          std::move(response_head), resource_load_info_->request_destination,
          is_ad_resource_));
}

void ResourceLoadInfoNotifierWrapper::NotifyResourceTransferSizeUpdated(
    int32_t transfer_size_diff) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (task_runner_->BelongsToCurrentThread()) {
    if (weak_wrapper_resource_load_info_notifier_) {
      weak_wrapper_resource_load_info_notifier_
          ->NotifyResourceTransferSizeUpdated(resource_load_info_->request_id,
                                              transfer_size_diff);
    }
    return;
  }
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &mojom::ResourceLoadInfoNotifier::NotifyResourceTransferSizeUpdated,
          weak_wrapper_resource_load_info_notifier_,
          resource_load_info_->request_id, transfer_size_diff));
}

void ResourceLoadInfoNotifierWrapper::NotifyResourceLoadCompleted(
    const network::URLLoaderCompletionStatus& status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RecordLoadHistograms(url::Origin::Create(resource_load_info_->final_url),
                       resource_load_info_->request_destination,
                       status.error_code);

  resource_load_info_->was_cached = status.exists_in_cache;
  resource_load_info_->net_error = status.error_code;
  resource_load_info_->total_received_bytes = status.encoded_data_length;
  resource_load_info_->raw_body_bytes = status.encoded_body_length;

  if (task_runner_->BelongsToCurrentThread()) {
    if (weak_wrapper_resource_load_info_notifier_) {
      weak_wrapper_resource_load_info_notifier_->NotifyResourceLoadCompleted(
          std::move(resource_load_info_), status);
    }
    return;
  }
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &mojom::ResourceLoadInfoNotifier::NotifyResourceLoadCompleted,
          weak_wrapper_resource_load_info_notifier_,
          std::move(resource_load_info_), status));
}

void ResourceLoadInfoNotifierWrapper::NotifyResourceLoadCanceled(
    int net_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RecordLoadHistograms(url::Origin::Create(resource_load_info_->final_url),
                       resource_load_info_->request_destination, net_error);

  if (task_runner_->BelongsToCurrentThread()) {
    if (weak_wrapper_resource_load_info_notifier_) {
      weak_wrapper_resource_load_info_notifier_->NotifyResourceLoadCanceled(
          resource_load_info_->request_id);
    }
    return;
  }
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &mojom::ResourceLoadInfoNotifier::NotifyResourceLoadCanceled,
          weak_wrapper_resource_load_info_notifier_,
          resource_load_info_->request_id));
}

}  // namespace blink
