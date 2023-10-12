// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONTENT_INDEX_CONTENT_INDEX_CONTEXT_IMPL_H_
#define CONTENT_BROWSER_CONTENT_INDEX_CONTENT_INDEX_CONTEXT_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "content/browser/content_index/content_index_database.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_index_context.h"
#include "third_party/blink/public/mojom/content_index/content_index.mojom.h"

namespace content {

class BrowserContext;
class ContentIndexProvider;
class ServiceWorkerContextWrapper;

// Owned by the Storage Partition. Components that want to query or modify the
// Content Index database should hold a reference to this.
class ContentIndexContextImpl
    : public ContentIndexContext,
      public base::RefCountedThreadSafe<ContentIndexContextImpl> {
 public:
  ContentIndexContextImpl(
      BrowserContext* browser_context,
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context);

  ContentIndexContextImpl(const ContentIndexContextImpl&) = delete;
  ContentIndexContextImpl& operator=(const ContentIndexContextImpl&) = delete;

  void Shutdown();

  // Queries the provider for the icon sizes needed to display the info.
  // Must be called on the UI thread.
  void GetIconSizes(
      blink::mojom::ContentCategory category,
      blink::mojom::ContentIndexService::GetIconSizesCallback callback);

  ContentIndexDatabase& database();

  // ContentIndexContent implementation.
  void GetIcons(int64_t service_worker_registration_id,
                const std::string& description_id,
                GetIconsCallback callback) override;
  void GetAllEntries(GetAllEntriesCallback callback) override;
  void GetEntry(int64_t service_worker_registration_id,
                const std::string& description_id,
                GetEntryCallback callback) override;
  void OnUserDeletedItem(int64_t service_worker_registration_id,
                         const url::Origin& origin,
                         const std::string& description_id) override;

 private:
  friend class base::RefCountedThreadSafe<ContentIndexContextImpl>;

  ~ContentIndexContextImpl() override;

  raw_ptr<ContentIndexProvider> provider_;
  ContentIndexDatabase content_index_database_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONTENT_INDEX_CONTENT_INDEX_CONTEXT_IMPL_H_
