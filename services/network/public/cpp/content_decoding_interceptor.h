// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CONTENT_DECODING_INTERCEPTOR_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CONTENT_DECODING_INTERCEPTOR_H_

#include <memory>
#include <utility>
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

class NetworkService;

// Intercepts network requests to apply content decoding (e.g., gzip, brotli,
// zstd) to the response body.
class COMPONENT_EXPORT(NETWORK_CPP) ContentDecodingInterceptor {
 public:
  // Convenience type alias for the data pipe handle pair.
  using DataPipePair = std::pair<mojo::ScopedDataPipeProducerHandle,
                                 mojo::ScopedDataPipeConsumerHandle>;

  // Identifies the component or context initiating the content decoding that
  // requires a data pipe. Used for categorizing UMA metrics.
  // LINT.IfChange(ContentDecodingInterceptorClientType)
  enum class ClientType {
    kTest,
    kURLLoaderThrottle,
    kCommitNavigation,
    kDownload,
    kNavigationPreload,
    kSignedExchange,
    kMaxValue = kSignedExchange,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/network/histograms.xml:ContentDecodingInterceptorClientType)

  // Creates the Mojo data pipe pair (producer/consumer) used by the
  // interceptor. Also handles test-only failure simulation and UMA logging for
  // results.
  //
  // On success, this function returns an `std::optional<DataPipePair>`
  // containing the pipe handles. On failure, it returns `std::nullopt`.
  // Failure occurs if `mojo::CreateDataPipe` itself fails (e.g., due to
  // resource exhaustion) or if failure is forced via the
  // `kRendererSideContentDecodingForceMojoFailureForTesting` feature parameter
  // for testing.
  //
  // Callers MUST check the return value for `std::nullopt` and handle the
  // failure case gracefully (e.g., report net::ERR_INSUFFICIENT_RESOURCES).
  static std::optional<DataPipePair> CreateDataPipePair(ClientType client_type);

  // Intercepts a URLLoader and its associated client, applying content decoding
  // to the response body. The decoding is performed on the passed
  // `worker_task_runner`. The provided `endpoints` and `body` are modified to
  // connect the client to the decoding interceptor.
  // Requires a valid `data_pipe_pair` (obtained from `CreateDataPipePair`)
  // which connects the interceptor's output to the original client's input.
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
  static void Intercept(
      const std::vector<net::SourceStreamType>& types,
      network::mojom::URLLoaderClientEndpointsPtr& endpoints,
      mojo::ScopedDataPipeConsumerHandle& body,
      DataPipePair data_pipe_pair,
      scoped_refptr<base::SequencedTaskRunner> worker_task_runner);

  // Intercepts a URLLoader and its associated client, applying content decoding
  // to the response body. The decoding is performed on the passed
  // `worker_task_runner`. This version uses a callback to swap the
  // URLLoaderClientEndpoints and data pipe, rather than modifying them
  // directly. This is useful when integrating with
  // blink::URLLoaderThrottle::Delegate's InterceptResponse() method.
  // Requires a valid `data_pipe_pair` (obtained from `CreateDataPipePair`)
  // which connects the interceptor's output to the original client's input.
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
  static void Intercept(
      const std::vector<net::SourceStreamType>& types,
      DataPipePair data_pipe_pair,
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
  // Requires a valid `data_pipe_pair` (obtained from `CreateDataPipePair`)
  // which connects the interceptor's output to the original client's input.
  //
  // The actual decoding work happens safely within the network service process.
  static void InterceptOnNetworkService(
      mojom::NetworkService& network_service,
      const std::vector<net::SourceStreamType>& types,
      network::mojom::URLLoaderClientEndpointsPtr& endpoints,
      mojo::ScopedDataPipeConsumerHandle& body,
      DataPipePair data_pipe_pair);

  // A capability class used as a key to restrict calls to
  // SetIsNetworkServiceRunningInTheCurrentProcess.
  class SetIsNetworkServiceRunningInTheCurrentProcessKey {
   private:
    SetIsNetworkServiceRunningInTheCurrentProcessKey() = default;
    friend class ::network::NetworkService;
  };
  // Sets a static flag indicating whether the Network Service is running within
  // the current process.
  static void SetIsNetworkServiceRunningInTheCurrentProcess(
      bool value,
      SetIsNetworkServiceRunningInTheCurrentProcessKey key);

 private:
  // Returns true if content decoding is permitted in the current process.
  // Decoding is allowed in non-browser processes. In the browser process,
  // it's only allowed if the Network Service is also running in-process.
  static bool IsInContentDecodingAllowedProcess();

  // Flag indicating if the network service is running in the current process.
  static bool is_network_serice_runnning_in_the_current_process_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CONTENT_DECODING_INTERCEPTOR_H_
