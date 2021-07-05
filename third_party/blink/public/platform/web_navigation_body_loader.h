// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_NAVIGATION_BODY_LOADER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_NAVIGATION_BODY_LOADER_H_

#include "base/containers/span.h"
#include "base/time/time.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "services/network/public/mojom/url_loader.mojom-forward.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom-forward.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom-forward.h"
#include "third_party/blink/public/platform/web_loader_freeze_mode.h"
#include "third_party/blink/public/platform/web_url_error.h"

namespace blink {

class ResourceLoadInfoNotifierWrapper;
struct WebNavigationParams;

// This class is used to load the body of main resource during navigation.
// It is provided by the client which commits a navigation.
// See WebNavigationParams for more details.
class BLINK_EXPORT WebNavigationBodyLoader {
 public:
  class Client {
   public:
    virtual ~Client() {}

    // Notifies about code cache if available. This method will
    // be called zero or one time.
    virtual void BodyCodeCacheReceived(mojo_base::BigBuffer data) = 0;

    // Notifies about more data available. Called multiple times.
    // If main resource is empty, can be not called at all.
    virtual void BodyDataReceived(base::span<const char> data) = 0;

    // Called once at the end. If something went wrong, |error| will be set.
    // No more calls are issued after this one.
    virtual void BodyLoadingFinished(
        base::TimeTicks completion_time,
        int64_t total_encoded_data_length,
        int64_t total_encoded_body_length,
        int64_t total_decoded_body_length,
        bool should_report_corb_blocking,
        const absl::optional<WebURLError>& error) = 0;
  };

  // This method fills navigation params related to the navigation request,
  // redirects and response, and also creates a body loader if needed.
  static void FillNavigationParamsResponseAndBodyLoader(
      mojom::CommonNavigationParamsPtr common_params,
      mojom::CommitNavigationParamsPtr commit_params,
      int request_id,
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle response_body,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      std::unique_ptr<ResourceLoadInfoNotifierWrapper>
          resource_load_info_notifier_wrapper,
      bool is_main_frame,
      WebNavigationParams* navigation_params);

  // It should be safe to destroy WebNavigationBodyLoader at any moment,
  // including from inside any client notification.
  virtual ~WebNavigationBodyLoader() {}

  // While frozen, data will be read on the renderer side but will not invoke
  // any web-exposed behavior such as dispatching messages or handling
  // redirects. This method can be called multiple times at any moment.
  virtual void SetDefersLoading(WebLoaderFreezeMode mode) = 0;

  // Starts loading the body. Client must be non-null, and will receive
  // the body, code cache and final result.
  virtual void StartLoadingBody(Client*,
                                mojom::CodeCacheHost* code_cache_host) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_NAVIGATION_BODY_LOADER_H_
