// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_CORS_CORS_URL_LOADER_TEST_UTIL_H_
#define SERVICES_NETWORK_CORS_CORS_URL_LOADER_TEST_UTIL_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/isolation_info.h"
#include "net/http/http_request_headers.h"
#include "net/log/test_net_log.h"
#include "services/network/public/cpp/cors/origin_access_list.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/client_security_state.mojom-forward.h"
#include "services/network/public/mojom/cors_origin_pattern.mojom-forward.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/resource_scheduler/resource_scheduler.h"
#include "services/network/test/test_url_loader_network_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

class GURL;

namespace net {

struct RedirectInfo;
struct MutableNetworkTrafficAnnotationTag;
struct NetLogEntry;
enum class NetLogEventType;
class URLRequestContext;

}  // namespace net

namespace network {

struct CorsErrorStatus;
class MockDevToolsObserver;
class NetworkContext;
class NetworkService;
class PrefetchMatchingURLLoaderFactory;
class TestURLLoaderClient;

namespace cors {

// TEST URL LOADER FACTORY
// =======================

class TestURLLoaderFactory : public mojom::URLLoaderFactory {
 public:
  TestURLLoaderFactory();

  TestURLLoaderFactory(const TestURLLoaderFactory&) = delete;
  TestURLLoaderFactory& operator=(const TestURLLoaderFactory&) = delete;

  ~TestURLLoaderFactory() override;

  base::WeakPtr<TestURLLoaderFactory> GetWeakPtr();

  void NotifyClientOnReceiveEarlyHints(
      const std::vector<std::pair<std::string, std::string>>& headers);

  void NotifyClientOnReceiveResponse(
      int status_code,
      const std::vector<std::pair<std::string, std::string>>& extra_headers,
      mojo::ScopedDataPipeConsumerHandle body);

  void NotifyClientOnComplete(int error_code);

  void NotifyClientOnComplete(const CorsErrorStatus& status);

  void NotifyClientOnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      const std::vector<std::pair<std::string, std::string>>& extra_headers);

  bool IsCreateLoaderAndStartCalled() { return !!client_remote_; }

  void SetOnCreateLoaderAndStart(const base::RepeatingClosure& closure) {
    on_create_loader_and_start_ = closure;
  }

  // Resets `client_remote_` to simulate an abort from the network side.
  void ResetClientRemote();

  const ResourceRequest& request() const { return request_; }
  const GURL& GetRequestedURL() const { return request_.url; }
  int num_created_loaders() const { return num_created_loaders_; }

 private:
  // mojom::URLLoaderFactory implementation.
  void CreateLoaderAndStart(mojo::PendingReceiver<mojom::URLLoader> receiver,
                            int32_t request_id,
                            uint32_t options,
                            const ResourceRequest& resource_request,
                            mojo::PendingRemote<mojom::URLLoaderClient> client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override;
  void Clone(mojo::PendingReceiver<mojom::URLLoaderFactory> receiver) override;

  mojo::Remote<mojom::URLLoaderClient> client_remote_;

  ResourceRequest request_;

  int num_created_loaders_ = 0;

  base::RepeatingClosure on_create_loader_and_start_;

  base::WeakPtrFactory<TestURLLoaderFactory> weak_factory_{this};
};

// CORS URL LOADER TEST BASE
// =========================

class CorsURLLoaderTestBase : public testing::Test {
 public:
  explicit CorsURLLoaderTestBase(bool shared_dictionary_enabled = false);
  ~CorsURLLoaderTestBase() override;

  CorsURLLoaderTestBase(const CorsURLLoaderTestBase&) = delete;
  CorsURLLoaderTestBase& operator=(const CorsURLLoaderTestBase&) = delete;

 protected:
  // A process ID attributed to a renderer process. See `ResetFactory()`.
  static constexpr uint32_t kRendererProcessId = 573;

  // A header that is exempt from the usual CORS rules.
  static constexpr char kTestCorsExemptHeader[] = "x-test-cors-exempt";

  // Optional parameters for `ResetFactory()`.
  struct ResetFactoryParams {
    // Sets each member to the default value of the corresponding mojom member.
    ResetFactoryParams();
    ~ResetFactoryParams();

    // Members of `mojom::URLLoaderFactoryParams`.
    bool is_trusted;
    bool ignore_isolated_world_origin;
    mojom::ClientSecurityStatePtr client_security_state;

    // Member of `mojom::URLLoaderFactoryOverride`.
    bool skip_cors_enabled_scheme_check;

    net::IsolationInfo isolation_info;

    mojo::PendingRemote<mojom::URLLoaderNetworkServiceObserver>
        url_loader_network_observer;
  };

  void CreateLoaderAndStart(
      const GURL& origin,
      const GURL& url,
      mojom::RequestMode mode,
      mojom::RedirectMode redirect_mode = mojom::RedirectMode::kFollow,
      mojom::CredentialsMode credentials_mode = mojom::CredentialsMode::kOmit);

  void CreateLoaderAndStart(const ResourceRequest& request);

  // Methods forwarded to the underlying `TestURLLoaderFactory`.

  bool IsNetworkLoaderStarted() {
    DCHECK(test_url_loader_factory_);
    return test_url_loader_factory_->IsCreateLoaderAndStartCalled();
  }

  void NotifyLoaderClientOnReceiveEarlyHints(
      const std::vector<std::pair<std::string, std::string>>& headers = {}) {
    DCHECK(test_url_loader_factory_);
    test_url_loader_factory_->NotifyClientOnReceiveEarlyHints(headers);
  }

  void NotifyLoaderClientOnReceiveResponse(
      const std::vector<std::pair<std::string, std::string>>& extra_headers =
          {},
      mojo::ScopedDataPipeConsumerHandle body =
          mojo::ScopedDataPipeConsumerHandle()) {
    DCHECK(test_url_loader_factory_);
    test_url_loader_factory_->NotifyClientOnReceiveResponse(200, extra_headers,
                                                            std::move(body));
  }

  void NotifyLoaderClientOnReceiveResponse(
      int status_code,
      const std::vector<std::pair<std::string, std::string>>& extra_headers =
          {},
      mojo::ScopedDataPipeConsumerHandle body =
          mojo::ScopedDataPipeConsumerHandle()) {
    DCHECK(test_url_loader_factory_);
    test_url_loader_factory_->NotifyClientOnReceiveResponse(
        status_code, extra_headers, std::move(body));
  }

  void NotifyLoaderClientOnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      const std::vector<std::pair<std::string, std::string>>& extra_headers =
          {}) {
    DCHECK(test_url_loader_factory_);
    test_url_loader_factory_->NotifyClientOnReceiveRedirect(redirect_info,
                                                            extra_headers);
  }

  void NotifyLoaderClientOnComplete(int error_code) {
    DCHECK(test_url_loader_factory_);
    test_url_loader_factory_->NotifyClientOnComplete(error_code);
  }

  void NotifyLoaderClientOnComplete(const CorsErrorStatus& status) {
    DCHECK(test_url_loader_factory_);
    test_url_loader_factory_->NotifyClientOnComplete(status);
  }

  const ResourceRequest& GetRequest() const {
    DCHECK(test_url_loader_factory_);
    return test_url_loader_factory_->request();
  }

  const GURL& GetRequestedURL() {
    DCHECK(test_url_loader_factory_);
    return test_url_loader_factory_->GetRequestedURL();
  }

  int num_created_loaders() const {
    DCHECK(test_url_loader_factory_);
    return test_url_loader_factory_->num_created_loaders();
  }

  // Resets `client_remote_` to simulate an abort from the network side.
  void ResetClientRemote() {
    DCHECK(test_url_loader_factory_);
    return test_url_loader_factory_->ResetClientRemote();
  }

  // Methods forwarded to the `CorsURLLoader` under test.

  void FollowRedirect(
      const std::vector<std::string>& removed_headers = {},
      const net::HttpRequestHeaders& modified_headers =
          net::HttpRequestHeaders(),
      const net::HttpRequestHeaders& modified_cors_exempt_headers =
          net::HttpRequestHeaders()) {
    DCHECK(url_loader_);
    url_loader_->FollowRedirect(removed_headers, modified_headers,
                                modified_cors_exempt_headers,
                                /*new_url=*/std::nullopt);
  }

  void AddHostHeaderAndFollowRedirect() {
    DCHECK(url_loader_);
    net::HttpRequestHeaders modified_headers;
    modified_headers.SetHeader(net::HttpRequestHeaders::kHost, "bar.test");
    url_loader_->FollowRedirect(/*removed_headers=*/{}, modified_headers,
                                /*modified_cors_exempt_headers=*/{},
                                /*new_url=*/std::nullopt);
  }

  // Methods for interacting with `TestURLLoaderClient`.

  const TestURLLoaderClient& client() const {
    return *test_cors_loader_client_;
  }

  void ClearHasReceivedRedirect();

  // These methods wait for events to happen asynchronously.
  void RunUntilCreateLoaderAndStartCalled();
  void RunUntilComplete();
  void RunUntilRedirectReceived();

  // These methods manipulate `origin_access_list_`.
  void AddAllowListEntryForOrigin(const url::Origin& source_origin,
                                  const std::string& protocol,
                                  const std::string& domain,
                                  const mojom::CorsDomainMatchMode mode);
  void AddBlockListEntryForOrigin(const url::Origin& source_origin,
                                  const std::string& protocol,
                                  const std::string& domain,
                                  const mojom::CorsDomainMatchMode mode);

  // Resets `cors_url_loader_factory_` with the given parameters.
  void ResetFactory(std::optional<url::Origin> initiator,
                    uint32_t process_id,
                    const ResetFactoryParams& params = ResetFactoryParams());

  NetworkContext* network_context() { return network_context_.get(); }

  void set_devtools_observer_for_next_request(MockDevToolsObserver* observer) {
    devtools_observer_for_next_request_ = observer;
  }

  // Working with the net log.

  // Returns the list of NetLog entries. All Observed NetLog entries may contain
  // logs of DNS config or Network quality. This function filters them.
  std::vector<net::NetLogEntry> GetEntries() const;

  // Returns a view of the types of the given entries, in the exact same order.
  static std::vector<net::NetLogEventType> GetTypesOfNetLogEntries(
      const std::vector<net::NetLogEntry>& entries);

  // Returns a pointer to the first entry in `entries` with the given `type`.
  // Returns nullptr if none can be found.
  const net::NetLogEntry* FindEntryByType(
      const std::vector<net::NetLogEntry>& entries,
      net::NetLogEventType type);

  static net::RedirectInfo CreateRedirectInfo(
      int status_code,
      std::string_view method,
      const GURL& url,
      std::string_view referrer = std::string_view(),
      net::ReferrerPolicy referrer_policy = net::ReferrerPolicy::NO_REFERRER,
      net::SiteForCookies site_for_cookies = net::SiteForCookies());

 private:
  // Test environment.
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<net::URLRequestContext> url_request_context_;
  ResourceScheduler resource_scheduler_;
  std::unique_ptr<NetworkService> network_service_;
  std::unique_ptr<NetworkContext> network_context_;
  mojo::Remote<mojom::NetworkContext> network_context_remote_;

  // Owner for the CorsURLLoaderFactory. Otherwise ignored by this class.
  std::unique_ptr<PrefetchMatchingURLLoaderFactory> factory_owner_;

  // `CorsURLLoaderFactory` instance under test.
  raw_ptr<mojom::URLLoaderFactory> cors_url_loader_factory_;
  mojo::Remote<mojom::URLLoaderFactory> cors_url_loader_factory_remote_;

  // The URL loader factory used inside `CorsURLLoader`.
  std::unique_ptr<TestURLLoaderFactory> test_url_loader_factory_;
  std::unique_ptr<mojo::Receiver<mojom::URLLoaderFactory>>
      test_url_loader_factory_receiver_;

  // Sets on the next request / passed to the next `CorsURLLoader`.
  raw_ptr<MockDevToolsObserver> devtools_observer_for_next_request_ = nullptr;

  // Holds URLLoader that CreateLoaderAndStart() creates.
  mojo::Remote<mojom::URLLoader> url_loader_;

  // TestURLLoaderClient that records callback activities.
  std::unique_ptr<TestURLLoaderClient> test_cors_loader_client_;

  // Holds for allowed origin access lists.
  OriginAccessList origin_access_list_;

  // Records net logs for verification in tests.
  net::RecordingNetLogObserver net_log_observer_;
};

}  // namespace cors
}  // namespace network

#endif  // SERVICES_NETWORK_CORS_CORS_URL_LOADER_TEST_UTIL_H_
