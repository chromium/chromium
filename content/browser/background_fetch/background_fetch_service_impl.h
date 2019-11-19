// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_SERVICE_IMPL_H_
#define CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_SERVICE_IMPL_H_

#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/browser/background_fetch/background_fetch_context.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/background_fetch/background_fetch.mojom.h"
#include "url/origin.h"

namespace content {

struct ServiceWorkerVersionInfo;

class CONTENT_EXPORT BackgroundFetchServiceImpl
    : public blink::mojom::BackgroundFetchService {
 public:
  BackgroundFetchServiceImpl(
      scoped_refptr<BackgroundFetchContext> background_fetch_context,
      url::Origin origin,
      int render_frame_tree_node_id,
      WebContents::Getter wc_getter);
  ~BackgroundFetchServiceImpl() override;

  static void CreateForWorker(
      const ServiceWorkerVersionInfo& info,
      mojo::PendingReceiver<blink::mojom::BackgroundFetchService> receiver);

  static void CreateForFrame(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::BackgroundFetchService> receiver);

  // blink::mojom::BackgroundFetchService implementation.
  void Fetch(int64_t service_worker_registration_id,
             const std::string& developer_id,
             std::vector<blink::mojom::FetchAPIRequestPtr> requests,
             blink::mojom::BackgroundFetchOptionsPtr options,
             const SkBitmap& icon,
             blink::mojom::BackgroundFetchUkmDataPtr ukm_data,
             FetchCallback callback) override;
  void GetIconDisplaySize(GetIconDisplaySizeCallback callback) override;
  void GetRegistration(int64_t service_worker_registration_id,
                       const std::string& developer_id,
                       GetRegistrationCallback callback) override;
  void GetDeveloperIds(int64_t service_worker_registration_id,
                       GetDeveloperIdsCallback callback) override;

 private:
  static void CreateOnCoreThread(
      scoped_refptr<BackgroundFetchContext> background_fetch_context,
      url::Origin origin,
      int render_frame_tree_node_id,
      WebContents::Getter wc_getter,
      mojo::PendingReceiver<blink::mojom::BackgroundFetchService> receiver);

  // Validates and returns whether the |developer_id|, |unique_id|, |requests|
  // and |title| respectively have valid values. The renderer will be flagged
  // for having sent a bad message if the values are invalid.
  bool ValidateDeveloperId(const std::string& developer_id) WARN_UNUSED_RESULT;
  bool ValidateUniqueId(const std::string& unique_id) WARN_UNUSED_RESULT;
  bool ValidateRequests(const std::vector<blink::mojom::FetchAPIRequestPtr>&
                            requests) WARN_UNUSED_RESULT;

  // The Background Fetch context on which operations will be dispatched.
  scoped_refptr<BackgroundFetchContext> background_fetch_context_;

  const url::Origin origin_;

  int render_frame_tree_node_id_;
  WebContents::Getter wc_getter_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_FETCH_BACKGROUND_FETCH_SERVICE_IMPL_H_
