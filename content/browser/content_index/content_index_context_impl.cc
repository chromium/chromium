// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/content_index/content_index_context_impl.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "url/origin.h"

namespace content {

ContentIndexContextImpl::ContentIndexContextImpl(
    BrowserContext* browser_context,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context)
    : provider_(browser_context->GetContentIndexProvider()),
      content_index_database_(browser_context,
                              std::move(service_worker_context)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void ContentIndexContextImpl::GetIcons(int64_t service_worker_registration_id,
                                       const std::string& description_id,
                                       GetIconsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content_index_database_.GetIcons(service_worker_registration_id,
                                   description_id, std::move(callback));
}

void ContentIndexContextImpl::GetAllEntries(GetAllEntriesCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content_index_database_.GetAllEntries(std::move(callback));
}

void ContentIndexContextImpl::GetEntry(int64_t service_worker_registration_id,
                                       const std::string& description_id,
                                       GetEntryCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content_index_database_.GetEntry(service_worker_registration_id,
                                   description_id, std::move(callback));
}

void ContentIndexContextImpl::OnUserDeletedItem(
    int64_t service_worker_registration_id,
    const url::Origin& origin,
    const std::string& description_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content_index_database_.DeleteItem(service_worker_registration_id, origin,
                                     description_id);
}

void ContentIndexContextImpl::GetIconSizes(
    blink::mojom::ContentCategory category,
    blink::mojom::ContentIndexService::GetIconSizesCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::vector<gfx::Size> icon_sizes;
  if (provider_)
    icon_sizes = provider_->GetIconSizes(category);

  std::move(callback).Run(std::move(icon_sizes));
}

void ContentIndexContextImpl::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  provider_ = nullptr;
  content_index_database_.Shutdown();
}

ContentIndexDatabase& ContentIndexContextImpl::database() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return content_index_database_;
}

ContentIndexContextImpl::~ContentIndexContextImpl() = default;

}  // namespace content
