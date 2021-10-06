// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_URL_LOADER_NAVIGATION_BODY_LOADER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_URL_LOADER_NAVIGATION_BODY_LOADER_H_

#include <stddef.h>
#include <stdint.h>

#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom-forward.h"
#include "third_party/blink/public/platform/web_loader_freeze_mode.h"
#include "third_party/blink/public/platform/web_navigation_body_loader.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace network {
struct URLLoaderCompletionStatus;
}  // namespace network

namespace blink {

class WebCodeCacheLoader;

// Navigation request is started in the browser process, and all redirects
// and final response are received there. Then we pass URLLoader and
// URLLoaderClient bindings to the renderer process, and create an instance
// of this class. It receives the response body, completion status and cached
// metadata, and dispatches them to Blink. It also ensures that completion
// status comes to Blink after the whole body was read and cached code metadata
// was received.
class NavigationBodyLoader : public WebNavigationBodyLoader,
                             public network::mojom::URLLoaderClient {
 public:
  NavigationBodyLoader(
      const KURL& original_url,
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle response_body,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      std::unique_ptr<ResourceLoadInfoNotifierWrapper>
          resource_load_info_notifier_wrapper,
      bool is_main_frame);
  ~NavigationBodyLoader() override;

 private:
  // The loading flow is outlined below. NavigationBodyLoader can be safely
  // deleted at any moment, and it will record cancelation stats, but will not
  // notify the client.
  //
  // StartLoadingBody
  //   request code cache
  // CodeCacheReceived
  //   notify client about cache
  // BindURLLoaderAndContinue
  // OnStartLoadingResponseBody
  //   start reading from the pipe
  // OnReadable (zero or more times)
  //   notify client about data
  // OnComplete (this might come before the whole body data is read,
  //             for example due to different mojo pipes being used
  //             without a relative order guarantee)
  //   save status for later use
  // OnReadable (zero or more times)
  //   notify client about data
  // NotifyCompletionIfAppropriate
  //   notify client about completion

  // WebNavigationBodyLoader implementation.
  void SetDefersLoading(WebLoaderFreezeMode mode) override;
  void StartLoadingBody(WebNavigationBodyLoader::Client* client,
                        mojom::CodeCacheHost* code_cache_host) override;

  // network::mojom::URLLoaderClient implementation.
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override;
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr response_head) override;
  void OnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      network::mojom::URLResponseHeadPtr response_head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override;
  void OnReceiveCachedMetadata(mojo_base::BigBuffer data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle handle) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  void CodeCacheReceived(base::TimeTicks start_time,
                         base::Time response_head_response_time,
                         base::Time response_time,
                         mojo_base::BigBuffer data);
  void BindURLLoaderAndContinue();
  void OnConnectionClosed();
  void OnReadable(MojoResult unused);
  // This method reads data from the pipe in a cycle and dispatches
  // BodyDataReceived synchronously.
  void ReadFromDataPipe();
  void NotifyCompletionIfAppropriate();
  void BindURLLoaderAndStartLoadingResponseBodyIfPossible();

  NavigationBodyLoader& operator=(const NavigationBodyLoader&) = delete;
  NavigationBodyLoader(const NavigationBodyLoader&) = delete;

  // Navigation parameters.
  network::mojom::URLResponseHeadPtr response_head_;
  mojo::ScopedDataPipeConsumerHandle response_body_;
  network::mojom::URLLoaderClientEndpointsPtr endpoints_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // These bindings are live while loading the response.
  mojo::Remote<network::mojom::URLLoader> url_loader_;
  mojo::Receiver<network::mojom::URLLoaderClient> url_loader_client_receiver_{
      this};
  WebNavigationBodyLoader::Client* client_ = nullptr;

  // The handle and watcher are live while loading the body.
  mojo::ScopedDataPipeConsumerHandle handle_;
  mojo::SimpleWatcher handle_watcher_;

  // This loader is live while retrieving the code cache.
  std::unique_ptr<WebCodeCacheLoader> code_cache_loader_;

  // Used to notify the navigation loading stats.
  std::unique_ptr<ResourceLoadInfoNotifierWrapper>
      resource_load_info_notifier_wrapper_;

  // The final status received from network or cancelation status if aborted.
  network::URLLoaderCompletionStatus status_;

  // Whether we got the body handle to read data from.
  bool has_received_body_handle_ = false;
  // Whether we got the final status.
  bool has_received_completion_ = false;
  // Whether we got all the body data.
  bool has_seen_end_of_data_ = false;

  // Frozen body loader does not send any notifications to the client
  // and tries not to read from the body pipe.
  WebLoaderFreezeMode freeze_mode_ = WebLoaderFreezeMode::kNone;

  // This protects against reentrancy into OnReadable,
  // which can happen due to nested message loop triggered
  // from iniside BodyDataReceived client notification.
  bool is_in_on_readable_ = false;

  // The original navigation url to start with.
  const KURL original_url_;

  const bool is_main_frame_;

  base::WeakPtrFactory<NavigationBodyLoader> weak_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_URL_LOADER_NAVIGATION_BODY_LOADER_H_
