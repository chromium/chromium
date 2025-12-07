// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/content_index/content_index_service_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "content/browser/bad_message.h"
#include "content/browser/content_index/content_index_database.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/service_worker_version_base_info.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

namespace content {

// static
void ContentIndexServiceImpl::CreateForFrame(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::ContentIndexService> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderProcessHost* render_process_host = render_frame_host->GetProcess();
  DCHECK(render_process_host);

  if (render_frame_host->IsNestedWithinFencedFrame()) {
    mojo::ReportBadMessage(
        "Content Index API is not allowed in a fenced frame");
    return;
  }

  auto* storage_partition = static_cast<StoragePartitionImpl*>(
      render_process_host->GetStoragePartition());

  mojo::MakeSelfOwnedReceiver(
      std::make_unique<ContentIndexServiceImpl>(
          render_frame_host->GetLastCommittedOrigin(),
          storage_partition->GetContentIndexContext(),
          render_frame_host->GetParentOrOuterDocument() == nullptr),
      std::move(receiver));
}

// static
void ContentIndexServiceImpl::CreateForWorker(
    const ServiceWorkerVersionBaseInfo& info,
    mojo::PendingReceiver<blink::mojom::ContentIndexService> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderProcessHost* render_process_host =
      RenderProcessHost::FromID(info.process_id);

  if (!render_process_host)
    return;

  auto* storage_partition = static_cast<StoragePartitionImpl*>(
      render_process_host->GetStoragePartition());

  scoped_refptr<ServiceWorkerRegistration> registration =
      storage_partition->GetServiceWorkerContext()->GetLiveRegistration(
          info.registration_id);
  if (registration && registration->ancestor_frame_type() ==
                          blink::mojom::AncestorFrameType::kFencedFrame) {
    mojo::ReportBadMessage(
        "Content Index API is not allowed in a fenced frame");
    return;
  }

  mojo::MakeSelfOwnedReceiver(std::make_unique<ContentIndexServiceImpl>(
                                  info.storage_key.origin(),
                                  storage_partition->GetContentIndexContext(),
                                  info.storage_key.IsFirstPartyContext()),
                              std::move(receiver));
}

ContentIndexServiceImpl::ContentIndexServiceImpl(
    const url::Origin& origin,
    scoped_refptr<ContentIndexContextImpl> content_index_context,
    bool is_top_level_context)
    : origin_(origin),
      content_index_context_(std::move(content_index_context)),
      is_top_level_context_(is_top_level_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

ContentIndexServiceImpl::~ContentIndexServiceImpl() = default;

void ContentIndexServiceImpl::GetIconSizes(
    blink::mojom::ContentCategory category,
    GetIconSizesCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content_index_context_->GetIconSizes(category, std::move(callback));
}

void ContentIndexServiceImpl::Add(
    int64_t service_worker_registration_id,
    blink::mojom::ContentDescriptionPtr description,
    const std::vector<SkBitmap>& icons,
    const GURL& launch_url,
    AddCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (const auto& icon : icons) {
    if (icon.isNull() || icon.width() * icon.height() > kMaxIconResolution) {
      mojo::ReportBadMessage("Invalid icon");
      std::move(callback).Run(
          blink::mojom::ContentIndexError::INVALID_PARAMETER);
      return;
    }
  }

  if (!launch_url.is_valid() || !origin_.IsSameOriginWith(launch_url)) {
    mojo::ReportBadMessage("Invalid launch URL");
    std::move(callback).Run(blink::mojom::ContentIndexError::INVALID_PARAMETER);
    return;
  }

  content_index_context_->database().AddEntry(
      service_worker_registration_id, origin_, is_top_level_context_,
      std::move(description), icons, launch_url, std::move(callback));
}

void ContentIndexServiceImpl::Delete(int64_t service_worker_registration_id,
                                     const std::string& content_id,
                                     DeleteCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content_index_context_->database().DeleteEntry(
      service_worker_registration_id, origin_, content_id, std::move(callback));
}

void ContentIndexServiceImpl::GetDescriptions(
    int64_t service_worker_registration_id,
    GetDescriptionsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content_index_context_->database().GetDescriptions(
      service_worker_registration_id, origin_, std::move(callback));
}

}  // namespace content
