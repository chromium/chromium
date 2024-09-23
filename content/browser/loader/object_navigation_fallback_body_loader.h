// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_OBJECT_NAVIGATION_FALLBACK_BODY_LOADER_H_
#define CONTENT_BROWSER_LOADER_OBJECT_NAVIGATION_FALLBACK_BODY_LOADER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "content/public/browser/navigation_handle_user_data.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom-forward.h"
#include "third_party/blink/public/mojom/timing/resource_timing.mojom.h"

namespace content {

class NavigationRequest;

// Helper to read body of an <object> navigation that failed with an HTTP error.
// Per https://whatwg.org/C/iframe-embed-object.html#the-object-element, the
// browser should fire an error event and then execute the fallback steps.
//
// What the standard does not specify, but all browser seem to implement, is
// that the failed navigation should *also* add a resource timing entry.
// Unfortunately, this proves to be a bit interesting to implement using the
// existing codepaths:
//
// - Navigation code would like to enforce the invariant that asking a cross-RFH
//   navigation to commit always results in a committed navigation. This is
//   critical for fixing bugs like https://crbug.com/83834 correctly. Thus, the
//   failed navigation cannot simply be sent to the renderer to commit (because
//   it's not supposed to actually result in a committed navigation). This means
//   that all the existing plumbing for generating the resource timing info from
//   loading a navigation body cannot be reused.
// - The response body cannot simply be sent to the renderer with the <object>
//   element, as this could lead to CORS violations. It's unsafe to simply send
//   the response head of any redirect responess + the final redirect as well.
//
// This leads to a rather unsatisfactory implementation that requires
// duplicating significant amounts of the resource timing code in the browser.
// It would be nice to do better somehow in the future...
class ObjectNavigationFallbackBodyLoader
    : public NavigationHandleUserData<ObjectNavigationFallbackBodyLoader>,
      public network::mojom::URLLoaderClient,
      public mojo::DataPipeDrainer::Client {
 public:
  // `response_body` and `url_loader_client_endpoints` are used to drain the
  // responise body and calculate the body / data size needed for the
  // performance entry.
  //
  // `navigation_request` is used to track if the navigation request that
  // triggered this work is still alive after the response body is read: if the
  // NavigationRequest is gone, the helper will not request to render the
  // fallback content / report timing info.
  //
  // `completion_closure` is used to clean up the navigation request if the
  // response body is successfully loaded.
  static void CreateAndStart(
      NavigationRequest& navigation_request,
      mojo::ScopedDataPipeConsumerHandle response_body,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      base::OnceClosure completion_closure);

  ~ObjectNavigationFallbackBodyLoader() override;

 private:
  friend NavigationHandleUserData<ObjectNavigationFallbackBodyLoader>;
  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();

  ObjectNavigationFallbackBodyLoader(
      NavigationHandle& navigation_handle,
      mojo::ScopedDataPipeConsumerHandle response_body,
      network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
      base::OnceClosure completion_closure);

  void BodyLoadFailed();

  // URLLoaderClient overrides:
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr) override;
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr,
      mojo::ScopedDataPipeConsumerHandle body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override;
  void OnReceiveRedirect(const net::RedirectInfo&,
                         network::mojom::URLResponseHeadPtr) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  // DataPipeDrainer::Client overrides:
  void OnDataAvailable(base::span<const uint8_t> data) override;
  void OnDataComplete() override;

  const raw_ref<NavigationRequest> navigation_request_;
  // `url_loader_` must be kept alive while reading the response body.
  mojo::Remote<network::mojom::URLLoader> url_loader_;
  mojo::Receiver<network::mojom::URLLoaderClient> url_loader_client_receiver_;
  // `response_body_drainer_` will be reset to null when the response body is
  // completely drained.
  std::unique_ptr<mojo::DataPipeDrainer> response_body_drainer_;
  base::OnceClosure completion_closure_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_OBJECT_NAVIGATION_FALLBACK_BODY_LOADER_H_
