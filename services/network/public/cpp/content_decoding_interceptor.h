// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CONTENT_DECODING_INTERCEPTOR_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CONTENT_DECODING_INTERCEPTOR_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/filter/source_stream_type.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace base {
class SequencedTaskRunner;
}

namespace network {
namespace mojom {
class NetworkService;
}  // namespace mojom

// Intercepts network requests to apply content decoding (e.g., gzip, brotli,
// zstd) to the response body.
class COMPONENT_EXPORT(NETWORK_CPP) ContentDecodingInterceptor {
 public:
  // Intercepts a URLLoader and its associated client, applying content decoding
  // to the response body. The decoding is performed on the passed
  // `worker_task_runner`. The provided `endpoints` and `body` are modified to
  // connect the client to the decoding interceptor.
  // The decoding is performed in the reverse order of the `types` vector. The
  // `types` vector must not be empty, and must not contain
  // SourceStreamType::kNone or SourceStreamType::kUnknown.
  //
  // The created interceptor is owned by the returned `endpoints`'s `url_loader`
  // remote interface. So the caller must keep the returned `endpoints`'s
  // `url_loader` alive until the caller receives the OnComplete callback via
  // the returned `endpoints`'s `url_loader_client`.
  //
  // IMPORTANT NOTE: This method performs decoding, so it MUST NOT be used in
  // the browser process, other than the network service on Android.
  // TODO(crbug.com/407477261): Add CHECK() for it.
  static void Intercept(
      const std::vector<net::SourceStreamType>& types,
      network::mojom::URLLoaderClientEndpointsPtr& endpoints,
      mojo::ScopedDataPipeConsumerHandle& body,
      scoped_refptr<base::SequencedTaskRunner> worker_task_runner);

  // Intercepts a URLLoader and its associated client, applying content decoding
  // to the response body. The decoding is performed on the passed
  // `worker_task_runner`. This version uses a callback to swap the
  // URLLoaderClientEndpoints and data pipe, rather than modifying them
  // directly. This is useful when integrating with
  // blink::URLLoaderThrottle::Delegate's InterceptResponse() method.
  // The decoding is performed in the reverse order of the `types` vector.
  // The `types` vector must not be empty, and must not contain
  // SourceStreamType::kNone or SourceStreamType::kUnknown.
  //
  // The created interceptor is owned by the returned `endpoints`'s `url_loader`
  // remote interface. So the caller must keep the returned `endpoints`'s
  // `url_loader` alive until the caller receives the OnComplete callback via
  // the returned `endpoints`'s `url_loader_client`.
  //
  // IMPORTANT NOTE: This method performs decoding, so it MUST NOT be used in
  // the browser process, other than the network service on Android.
  // TODO(crbug.com/407477261): Add CHECK() for it.
  static void Intercept(
      const std::vector<net::SourceStreamType>& types,
      base::OnceCallback<
          void(network::mojom::URLLoaderClientEndpointsPtr& endpoints,
               mojo::ScopedDataPipeConsumerHandle& body)> swap_callback,
      scoped_refptr<base::SequencedTaskRunner> worker_task_runner);

  // Intercepts a URLLoader and its associated client, applying content decoding
  // to the response body. The decoding is performed on the passed
  // `worker_task_runner`. This version is useful when a
  // ScopedDataPipeProducerHandle is provided by the caller side.
  //
  // IMPORTANT NOTE: This method performs decoding, so it MUST NOT be used in
  // the browser process, other than the network service on Android.
  // TODO(crbug.com/407477261): Add CHECK() for it.
  static void Intercept(
      const std::vector<net::SourceStreamType>& types,
      mojo::ScopedDataPipeConsumerHandle source_body,
      mojo::ScopedDataPipeProducerHandle dest_body,
      mojo::PendingRemote<network::mojom::URLLoader> source_url_loader,
      mojo::PendingReceiver<network::mojom::URLLoaderClient>
          source_url_loader_client,
      mojo::PendingReceiver<network::mojom::URLLoader> dest_url_loader,
      mojo::PendingRemote<network::mojom::URLLoaderClient>
          dest_url_loader_client,
      scoped_refptr<base::SequencedTaskRunner> worker_task_runner);

  // Requests the network service process to intercept a URLLoader connection
  // and perform content decoding based on the specified `types`.
  //
  // This method is intended for use by the browser process when it needs
  // decoding for a response (e.g., for downloads or Signed Exchanges) even
  // though client-side decoding might have been initially requested for the
  // original load. It achieves this by calling the
  // `NetworkService::InterceptUrlLoaderForBodyDecoding` Mojo method.
  //
  // It creates new data pipes and replaces the caller's `endpoints` and `body`
  // handles with new ones representing the output of the interceptor (which
  // runs in the network service). The actual decoding work happens safely
  // within the network service process.
  static void InterceptOnNetworkService(
      mojom::NetworkService& network_service,
      const std::vector<net::SourceStreamType>& types,
      network::mojom::URLLoaderClientEndpointsPtr& endpoints,
      mojo::ScopedDataPipeConsumerHandle& body);

  // For testing purposes only. If set to true, the creation of the Mojo data
  // pipe within this class's methods will be forced to fail, simulating an
  // insufficient resources error (`net::ERR_INSUFFICIENT_RESOURCES`).
  static void SetForceMojoCreateDataPipeFailureForTesting(bool value);

 private:
  // Backing flag for the test utility above. Defined in the .cc file.
  static bool force_mojo_create_data_pipe_failure_for_testing_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CONTENT_DECODING_INTERCEPTOR_H_
