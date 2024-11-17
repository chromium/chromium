// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONTENT_INDEX_CONTENT_INDEX_SERVICE_IMPL_H_
#define CONTENT_BROWSER_CONTENT_INDEX_CONTENT_INDEX_SERVICE_IMPL_H_

#include "base/memory/scoped_refptr.h"
#include "content/browser/content_index/content_index_context_impl.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/content_index/content_index.mojom.h"
#include "url/origin.h"

class GURL;

namespace content {

class RenderFrameHost;
struct ServiceWorkerVersionBaseInfo;

// Lazily constructed by the corresponding renderer when the Content Index API
// is triggered.
class CONTENT_EXPORT ContentIndexServiceImpl
    : public blink::mojom::ContentIndexService {
 public:
  static void CreateForFrame(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::ContentIndexService> receiver);

  static void CreateForWorker(
      const ServiceWorkerVersionBaseInfo& info,
      mojo::PendingReceiver<blink::mojom::ContentIndexService> receiver);

  ContentIndexServiceImpl(
      const url::Origin& origin,
      scoped_refptr<ContentIndexContextImpl> content_index_context,
      bool is_top_level_context);

  ContentIndexServiceImpl(const ContentIndexServiceImpl&) = delete;
  ContentIndexServiceImpl& operator=(const ContentIndexServiceImpl&) = delete;

  ~ContentIndexServiceImpl() override;

  // blink::mojom::ContentIndexService implementation.
  void GetIconSizes(blink::mojom::ContentCategory category,
                    GetIconSizesCallback callback) override;
  void Add(int64_t service_worker_registration_id,
           blink::mojom::ContentDescriptionPtr description,
           const std::vector<SkBitmap>& icons,
           const GURL& launch_url,
           AddCallback callback) override;
  void Delete(int64_t service_worker_registration_id,
              const std::string& content_id,
              DeleteCallback callback) override;
  void GetDescriptions(int64_t service_worker_registration_id,
                       GetDescriptionsCallback callback) override;

 private:
  url::Origin origin_;
  scoped_refptr<ContentIndexContextImpl> content_index_context_;
  bool is_top_level_context_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONTENT_INDEX_CONTENT_INDEX_SERVICE_IMPL_H_
