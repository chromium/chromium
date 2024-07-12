// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_URL_LOADER_FACTORY_UTILS_H_
#define CONTENT_BROWSER_LOADER_URL_LOADER_FACTORY_UTILS_H_

#include "base/memory/stack_allocated.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/common/content_export.h"
#include "content/public/browser/content_browser_client.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"

namespace net {
class IsolationInfo;
}

namespace content {

class StoragePartitionImpl;

namespace url_loader_factory {

using Interceptor = base::RepeatingCallback<
    void(int process_id, network::URLLoaderFactoryBuilder& factory_builder)>;

// This method must be called on the UI thread.
CONTENT_EXPORT const Interceptor& GetTestingInterceptor();

// Allows intercepting the URLLoaderFactory creation.
// For every `SetInterceptorForTesting(non-null interceptor)`,
// `SetInterceptorForTesting({})` must be called to ensure restoring the default
// behavior.
// This method must be called either on the UI thread or before threads start.
// This callback is run on the UI thread.
// TODO(crbug.com/40947547): Document when the interception occurs.
CONTENT_EXPORT void SetInterceptorForTesting(const Interceptor& interceptor);

// Only accessed on the IO thread.
// Basically the same as `!!GetTestingInterceptor()`, and introduced to avoid
// possible race conditions between UI/IO threads.
CONTENT_EXPORT bool HasInterceptorOnIOThreadForTesting();
CONTENT_EXPORT void SetHasInterceptorOnIOThreadForTesting(bool has_interceptor);

// A parameter object for `ContentBrowserClient::WillCreateURLLoaderFactory()`.
class CONTENT_EXPORT ContentClientParams final {
  STACK_ALLOCATED();

 public:
  ContentClientParams(BrowserContext* browser_context,
                      RenderFrameHost* frame,
                      int render_process_id,
                      const url::Origin& request_initiator,
                      const net::IsolationInfo& isolation_info,
                      ukm::SourceIdObj ukm_source_id,
                      bool* bypass_redirect_checks = nullptr,
                      std::optional<int64_t> navigation_id = std::nullopt,
                      scoped_refptr<base::SequencedTaskRunner>
                          navigation_response_task_runner = nullptr);

  ContentClientParams(const ContentClientParams&) = delete;
  ContentClientParams& operator=(const ContentClientParams&) = delete;
  ContentClientParams(ContentClientParams&&);
  ContentClientParams& operator=(ContentClientParams&&);
  ~ContentClientParams();

  // Invokes `ContentBrowserClient::WillCreateURLLoaderFactory()` with the
  // parameters given to this method and the constructor.
  void Run(network::URLLoaderFactoryBuilder& factory_builder,
           ContentBrowserClient::URLLoaderFactoryType type,
           mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
               header_client,
           bool* disable_secure_dns,
           network::mojom::URLLoaderFactoryOverridePtr* factory_override);

 private:
  raw_ptr<BrowserContext> browser_context_;
  raw_ptr<RenderFrameHost> frame_;
  int render_process_id_;
  raw_ref<const url::Origin> request_initiator_;
  raw_ref<const net::IsolationInfo> isolation_info_;
  ukm::SourceIdObj ukm_source_id_;
  raw_ptr<bool> bypass_redirect_checks_;
  std::optional<int64_t> navigation_id_;
  scoped_refptr<base::SequencedTaskRunner> navigation_response_task_runner_;
};

// When `kAllow` is used, non-null `header_client`, `disable_secure_dns` and
// `factory_override` (respectively) are passed to
// `ContentBrowserClient::WillCreateURLLoaderFactory()` and
// `devtools_instrumentation`.
// Otherwise (`kDisallow`), nullptr are passed.
enum class HeaderClientOption { kAllow, kDisallow };
enum class DisableSecureDnsOption { kAllow, kDisallow };
enum class FactoryOverrideOption { kAllow, kDisallow };

// Specifies the final destination of the URLLoaderFactory in `Create()`.
class CONTENT_EXPORT TerminalParams final {
  STACK_ALLOCATED();

 public:
  // Connects to the URLLoaderFactory from
  // `NetworkContext::CreateURLLoaderFactory(factory_params)`.
  // Any options listed in the arguments can be specified.
  static TerminalParams ForNetworkContext(
      network::mojom::NetworkContext* network_context,
      network::mojom::URLLoaderFactoryParamsPtr factory_params,
      HeaderClientOption header_client_option = HeaderClientOption::kDisallow,
      FactoryOverrideOption factory_override_option =
          FactoryOverrideOption::kDisallow,
      DisableSecureDnsOption disable_secure_dns_option =
          DisableSecureDnsOption::kDisallow);

  // Connects to `storage_partition->GetURLLoaderFactoryForBrowserProcess()`
  // if possible, or otherwise fallback to
  // `storage_partition->GetNetworkContext()->CreateURLLoaderFactory(
  //      storage_partition->CreateURLLoaderFactoryParams())`.
  //
  // This is equivalent to
  // `TerminalParams::ForNetworkContext(
  //      storage_partition->GetNetworkContext(),
  //      storage_partition->CreateURLLoaderFactoryParams(), ...)`
  // except that for `ForBrowserProcess()`
  // -  `GetURLLoaderFactoryForBrowserProcess()` can be used if possible, while
  // - `FactoryOverrideOption`/`DisableSecureDnsOption` must be `kDisallow`.
  static TerminalParams ForBrowserProcess(
      StoragePartitionImpl* storage_partition,
      HeaderClientOption header_client_option = HeaderClientOption::kDisallow);

  // Connects to the given URLLoaderFactory(-ish) object.
  // This should be used only for requests not served by the network service,
  // e.g. requests with non-network schemes and requests served by prefetch
  // caches. For requests served by the network service, use
  // `ForNetworkContext()` or `ForBrowserProcess()` as they should have
  // corresponding `NetworkContext`s.
  //
  // See the `process_id_` comment below for `process_id`.
  using URLLoaderFactoryTypes =
      absl::variant<mojo::PendingRemote<network::mojom::URLLoaderFactory>,
                    scoped_refptr<network::SharedURLLoaderFactory>>;
  static TerminalParams ForNonNetwork(URLLoaderFactoryTypes url_loader_factory,
                                      int process_id);

  TerminalParams(TerminalParams&&);
  TerminalParams& operator=(TerminalParams&&);
  ~TerminalParams();

  network::mojom::NetworkContext* network_context() const;
  HeaderClientOption header_client_option() const;
  FactoryOverrideOption factory_override_option() const;
  DisableSecureDnsOption disable_secure_dns_option() const;
  StoragePartitionImpl* storage_partition() const;
  int process_id() const;
  network::mojom::URLLoaderFactoryParamsPtr TakeFactoryParams();
  std::optional<URLLoaderFactoryTypes> TakeURLLoaderFactory();

 private:
  TerminalParams(network::mojom::NetworkContext* network_context,
                 network::mojom::URLLoaderFactoryParamsPtr factory_params,
                 HeaderClientOption header_client_option,
                 FactoryOverrideOption factory_override_option,
                 DisableSecureDnsOption disable_secure_dns_option,
                 StoragePartitionImpl* storage_partition,
                 std::optional<URLLoaderFactoryTypes> url_loader_factory,
                 int process_id);

  raw_ptr<network::mojom::NetworkContext> network_context_;
  network::mojom::URLLoaderFactoryParamsPtr factory_params_;
  HeaderClientOption header_client_option_;
  FactoryOverrideOption factory_override_option_;
  DisableSecureDnsOption disable_secure_dns_option_;
  raw_ptr<StoragePartitionImpl> storage_partition_;
  std::optional<URLLoaderFactoryTypes> url_loader_factory_;

  // The process ID plumbed to URLLoaderInterceptor. This can be
  // - a renderer process, or
  // - `network::mojom::kBrowserProcessId` for browser process.
  // TODO(crbug.com/324458368): This is different from
  // `ContentClientParams::render_process_id_`. Clarify the meaning of
  // `process_id_` here if needed.
  int process_id_;
};

// Creates a URLLoaderFactory, intercepted by:
// 1. `ContentBrowserClient::WillCreateURLLoaderFactory()`
//    (if `content_client_params` is non-nullopt),
// 2. `devtools_instrumentation` (if `devtools_params` is non-nullopt)
// 3. `GetInterceptor()`
//    (see the comments in the .cc file for detailed conditions)
// and then finally routed as specified by `TerminalParams`.
//
// The created URLLoaderFactory is
// - Returned as `scoped_refptr<network::SharedURLLoaderFactory>`,
// - Returned as `mojo::PendingRemote<network::mojom::URLLoaderFactory>`, or
// - Connected to `receiver_to_connect`,
// respectively for the variants below.
//
// Note that the created URLLoaderFactory might NOT support auto-reconnect after
// a crash of Network Service.
[[nodiscard]] CONTENT_EXPORT scoped_refptr<network::SharedURLLoaderFactory>
Create(ContentBrowserClient::URLLoaderFactoryType type,
       TerminalParams terminal_params,
       std::optional<ContentClientParams> content_client_params = std::nullopt,
       std::optional<devtools_instrumentation::WillCreateURLLoaderFactoryParams>
           devtools_params = std::nullopt);

[[nodiscard]] CONTENT_EXPORT mojo::PendingRemote<
    network::mojom::URLLoaderFactory>
CreatePendingRemote(
    ContentBrowserClient::URLLoaderFactoryType type,
    TerminalParams terminal_params,
    std::optional<ContentClientParams> content_client_params = std::nullopt,
    std::optional<devtools_instrumentation::WillCreateURLLoaderFactoryParams>
        devtools_params = std::nullopt);

CONTENT_EXPORT void CreateAndConnectToPendingReceiver(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver_to_connect,
    ContentBrowserClient::URLLoaderFactoryType type,
    TerminalParams terminal_params,
    std::optional<ContentClientParams> content_client_params = std::nullopt,
    std::optional<devtools_instrumentation::WillCreateURLLoaderFactoryParams>
        devtools_params = std::nullopt);

}  // namespace url_loader_factory
}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_URL_LOADER_FACTORY_UTILS_H_
