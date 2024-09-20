// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_HOST_RESOLVER_H_
#define NET_DNS_HOST_RESOLVER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "base/containers/span.h"
#include "base/values.h"
#include "net/base/address_family.h"
#include "net/base/completion_once_callback.h"
#include "net/base/connection_endpoint_metadata.h"
#include "net/base/host_port_pair.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/network_handle.h"
#include "net/base/request_priority.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver_system_task.h"
#include "net/dns/public/dns_config_overrides.h"
#include "net/dns/public/dns_query_type.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/dns/public/host_resolver_source.h"
#include "net/dns/public/mdns_listener_update_type.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/log/net_log_with_source.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/scheme_host_port.h"

namespace net {

class AddressList;
class ContextHostResolver;
class DnsClient;
struct DnsConfigOverrides;
class HostResolverManager;
class NetLog;
class URLRequestContext;

template <typename T>
concept HasConnectionEndpointMetadata = requires(T t) {
  { t.metadata } -> std::same_as<ConnectionEndpointMetadata&>;
};

// This class represents the task of resolving hostnames (or IP address
// literal) to an AddressList object (or other DNS-style results).
//
// Typically implemented by ContextHostResolver or wrappers thereof. See
// HostResolver::Create[...]() methods for construction or URLRequestContext for
// retrieval.
//
// See mock_host_resolver.h for test implementations.
class NET_EXPORT HostResolver {
 public:
  class NET_EXPORT Host {
   public:
    explicit Host(absl::variant<url::SchemeHostPort, HostPortPair> host);
    ~Host();

    Host(const Host&);
    Host& operator=(const Host&);
    Host(Host&&);
    Host& operator=(Host&&);

    bool HasScheme() const;
    const std::string& GetScheme() const;
    std::string GetHostname() const;  // With brackets for IPv6 literals.
    std::string_view GetHostnameWithoutBrackets() const;
    uint16_t GetPort() const;

    std::string ToString() const;

    const url::SchemeHostPort& AsSchemeHostPort() const;

    bool operator==(const Host& other) const { return host_ == other.host_; }

    bool operator<(const Host& other) const { return host_ < other.host_; }

   private:
    absl::variant<url::SchemeHostPort, HostPortPair> host_;
  };

  // Handler for an individual host resolution request. Created by
  // HostResolver::CreateRequest().
  //
  // TODO(crbug.com/40212535): Most result retrieval here follows a pattern
  // where it may return null or empty for requests where that result type is
  // not available. Clean this up to always return empty for such cases and
  // remove nullability from the return types.
  class ResolveHostRequest {
   public:
    // Destruction cancels the request if running asynchronously, causing the
    // callback to never be invoked.
    virtual ~ResolveHostRequest() = default;

    // Starts the request and returns a network error code.
    //
    // If the request could not be handled synchronously, returns
    // |ERR_IO_PENDING|, and completion will be signaled later via |callback|.
    // On any other returned value, the request was handled synchronously and
    // |callback| will not be invoked.
    //
    // Results in ERR_NAME_NOT_RESOLVED if the hostname is not resolved. More
    // detail about the underlying error can be retrieved using
    // GetResolveErrorInfo().
    //
    // The parent HostResolver must still be alive when Start() is called,  but
    // if it is destroyed before an asynchronous result completes, the request
    // will be automatically cancelled.
    //
    // If cancelled before |callback| is invoked, it will never be invoked.
    virtual int Start(CompletionOnceCallback callback) = 0;

    // Address record (A or AAAA) results of the request. Should only be called
    // after Start() signals completion, either by invoking the callback or by
    // returning a result other than |ERR_IO_PENDING|. May return nullptr or
    // empty for non-address requests.
    //
    // TODO(crbug.com/40203587): Remove and replace all usage with
    // GetEndpointResults().
    virtual const AddressList* GetAddressResults() const = 0;

    // Endpoint results for `A`, `AAAA`, `UNSPECIFIED`, or `HTTPS` requests.
    // Should only be called after Start() signals completion, either by
    // invoking the callback or by returning a result other than
    // `ERR_IO_PENDING`. May return nullptr or empty for non-address/HTTPS
    // requests.
    virtual const std::vector<HostResolverEndpointResult>* GetEndpointResults()
        const = 0;

    // Text record (TXT) results of the request. Should only be called after
    // Start() signals completion, either by invoking the callback or by
    // returning a result other than |ERR_IO_PENDING|. May return nullptr or
    // empty for non-TXT requests.
    virtual const std::vector<std::string>* GetTextResults() const = 0;

    // Hostname record (SRV or PTR) results of the request. For SRV results,
    // hostnames are ordered according to their priorities and weights. See RFC
    // 2782. May return nullptr or empty for non-SRV/PTR requests.
    //
    // Should only be called after Start() signals completion, either by
    // invoking the callback or by returning a result other than
    // |ERR_IO_PENDING|.
    virtual const std::vector<HostPortPair>* GetHostnameResults() const = 0;

    // Any DNS record aliases, such as CNAME aliases, found as a result of an
    // address query. Includes all known aliases, e.g. from A, AAAA, or HTTPS,
    // not just from the address used for the connection, in no particular
    // order. Should only be called after Start() signals completion, either by
    // invoking the callback or by returning a result other than
    // `ERR_IO_PENDING`. Returns a list of aliases that has been fixed up and
    // canonicalized (as URL hostnames), and thus may differ from the results
    // stored directly in the AddressList. May return nullptr or empty for
    // non-address/HTTPS requests.
    //
    // If `ResolveHostParameters::include_canonical_name` was true, alias
    // results will always be the single "canonical name" received from the
    // system resolver without URL hostname canonicalization (or an empty set or
    // `nullptr` in the unusual case that the system resolver did not give a
    // canonical name).
    virtual const std::set<std::string>* GetDnsAliasResults() const = 0;

    // Result of an experimental query. Meaning depends on the specific query
    // type, but each boolean value generally refers to a valid or invalid
    // record of the experimental type. May return nullptr or empty for requests
    // without experimental result behavior.
    NET_EXPORT virtual const std::vector<bool>*
    GetExperimentalResultsForTesting() const;

    // Error info for the request.
    //
    // Should only be called after Start() signals completion, either by
    // invoking the callback or by returning a result other than
    // |ERR_IO_PENDING|.
    virtual ResolveErrorInfo GetResolveErrorInfo() const = 0;

    // Information about the result's staleness in the host cache. Only
    // available if results were received from the host cache, otherwise
    // returns nullopt.
    //
    // Should only be called after Start() signals completion, either by
    // invoking the callback or by returning a result other than
    // |ERR_IO_PENDING|.
    virtual const std::optional<HostCache::EntryStaleness>& GetStaleInfo()
        const = 0;

    // Changes the priority of the specified request. Can only be called while
    // the request is running (after Start() returns |ERR_IO_PENDING| and before
    // the callback is invoked).
    virtual void ChangeRequestPriority(RequestPriority priority) {}
  };

  // Handler for a service endpoint resolution request. Unlike
  // ResolveHostRequest, which waits for all responses, this could provide
  // intermediate endpoint candidates in the middle of the resolution.
  //
  // A client owns an instance of this class. Destruction cancels the request.
  class ServiceEndpointRequest {
   public:
    class Delegate {
     public:
      virtual ~Delegate() = default;

      // Called when the request has updated endpoints.
      virtual void OnServiceEndpointsUpdated() = 0;

      // Called when all queries are responded or an error occurred.
      // Note that this can be called without OnServiceEndpointsUpdated().
      virtual void OnServiceEndpointRequestFinished(int rv) = 0;
    };

    virtual ~ServiceEndpointRequest() = default;

    // Starts resolving service endpoints. `delegate` is used only when this
    // method returns ERR_IO_PENDING. When the return value is other than
    // ERR_IO_PENDING, resolution completed (or an error occurred)
    // synchronously, and GetEndpointResults() will return finalized results.
    virtual int Start(Delegate* delegate) = 0;

    // The current available service endpoints. These can be changed over time
    // while resolution is still ongoing. Changes are signaled by a call to the
    // delegate's OnServiceEndpointsUpdated(). Results are finalized when
    // Start() finished synchronously (returning other than ERR_IO_PENDING), or
    // delegate's OnServiceEndpointRequestFinished() is called.
    virtual const std::vector<ServiceEndpoint>& GetEndpointResults() = 0;

    // Any DNS record aliases, such as CNAME aliases, found as a result of
    // addresses and HTTPS queries. These can be changed over time while
    // resolution is still ongoing. See also the comment on
    // Request::GetDnsAliasResults() for details.
    virtual const std::set<std::string>& GetDnsAliasResults() = 0;

    // True if the client of this request can attempt cryptographic handshakes.
    // If false, the provided service endpoints via GetEndpointResults() are not
    // finalized to the point to allow completing transactions, and data or
    // cryptographic handshakes must not be sent. This can be changed over time
    // while resolution is still ongoing.
    // TODO(crbug.com/41493696): Consider renaming this to
    // `IsSvcbResolutionCompleted()` when Chrome supports HTTPS follow-up
    // queries.
    virtual bool EndpointsCryptoReady() = 0;

    // Returns the error info of this request. This can be changed over time
    // while resolution is still ongoing. In general, should be called only
    // after resolution completed.
    virtual ResolveErrorInfo GetResolveErrorInfo() = 0;

    // Change the priority of this request.
    virtual void ChangeRequestPriority(RequestPriority priority) = 0;
  };

  // Handler for an activation of probes controlled by a HostResolver. Created
  // by HostResolver::CreateDohProbeRequest().
  class ProbeRequest {
   public:
    // Destruction cancels the request and all probes.
    virtual ~ProbeRequest() = default;

    // Activates async running of probes. Always returns ERR_IO_PENDING or an
    // error from activating probes. No callback as probes will never "complete"
    // until cancellation.
    virtual int Start() = 0;
  };

  // The options for features::kUseDnsHttpsSvcb experiment. See the comments
  // in net/base/features.h for more details.
  struct NET_EXPORT HttpsSvcbOptions {
    HttpsSvcbOptions();
    HttpsSvcbOptions(const HttpsSvcbOptions&);
    HttpsSvcbOptions(HttpsSvcbOptions&&);
    HttpsSvcbOptions& operator=(const HttpsSvcbOptions&) = default;
    HttpsSvcbOptions& operator=(HttpsSvcbOptions&&) = default;
    ~HttpsSvcbOptions();

    static HttpsSvcbOptions FromDict(const base::Value::Dict& dict);
    static HttpsSvcbOptions FromFeatures();

    bool enable = false;
    base::TimeDelta insecure_extra_time_max;
    int insecure_extra_time_percent = 0;
    base::TimeDelta insecure_extra_time_min;
    base::TimeDelta secure_extra_time_max;
    int secure_extra_time_percent = 0;
    base::TimeDelta secure_extra_time_min;
  };

  // Parameter-grouping struct for additional optional parameters for creation
  // of HostResolverManagers and stand-alone HostResolvers.
  struct NET_EXPORT ManagerOptions {
    ManagerOptions();
    ManagerOptions(const ManagerOptions&);
    ManagerOptions(ManagerOptions&&);
    ManagerOptions& operator=(const ManagerOptions&) = default;
    ManagerOptions& operator=(ManagerOptions&&) = default;
    ~ManagerOptions();

    // Set |max_concurrent_resolves| to this to select a default level
    // of concurrency.
    static const size_t kDefaultParallelism = 0;

    // How many resolve requests will be allowed to run in parallel.
    // |kDefaultParallelism| for the resolver to choose a default value.
    size_t max_concurrent_resolves = kDefaultParallelism;

    // The maximum number of times to retry for host resolution if using the
    // system resolver. No effect when the system resolver is not used.
    // |kDefaultRetryAttempts| for the resolver to choose a default value.
    size_t max_system_retry_attempts =
        HostResolverSystemTask::Params::kDefaultRetryAttempts;

    // Initial setting for whether the insecure portion of the built-in
    // asynchronous DnsClient is enabled or disabled. See HostResolverManager::
    // SetInsecureDnsClientEnabled() for details.
    bool insecure_dns_client_enabled = false;

    // Initial setting for whether additional DNS types (e.g. HTTPS) may be
    // queried when using the built-in resolver for insecure DNS.
    bool additional_types_via_insecure_dns_enabled = true;

    // Initial configuration overrides for the built-in asynchronous DnsClient.
    // See HostResolverManager::SetDnsConfigOverrides() for details.
    DnsConfigOverrides dns_config_overrides;

    // If set to |false|, when on a WiFi connection, IPv6 will be assumed to be
    // unreachable without actually checking. See https://crbug.com/696569 for
    // further context.
    bool check_ipv6_on_wifi = true;

    // An experimental options for features::kUseDnsHttpsSvcb
    // and features::kUseDnsHttpsSvcbAlpn.
    std::optional<HostResolver::HttpsSvcbOptions> https_svcb_options;
  };

  // Factory class. Useful for classes that need to inject and override resolver
  // creation for tests.
  class NET_EXPORT Factory {
   public:
    virtual ~Factory() = default;

    // See HostResolver::CreateResolver.
    virtual std::unique_ptr<HostResolver> CreateResolver(
        HostResolverManager* manager,
        std::string_view host_mapping_rules,
        bool enable_caching);

    // See HostResolver::CreateStandaloneResolver.
    virtual std::unique_ptr<HostResolver> CreateStandaloneResolver(
        NetLog* net_log,
        const ManagerOptions& options,
        std::string_view host_mapping_rules,
        bool enable_caching);
  };

  // Parameter-grouping struct for additional optional parameters for
  // CreateRequest() calls. All fields are optional and have a reasonable
  // default.
  struct NET_EXPORT ResolveHostParameters {
    ResolveHostParameters();

    // Requested DNS query type. If UNSPECIFIED, the resolver will select a set
    // of queries automatically. It will select A, AAAA, or both as the address
    // queries, depending on IPv4/IPv6 settings and reachability. It may also
    // replace UNSPECIFIED with additional queries, such as HTTPS.
    DnsQueryType dns_query_type = DnsQueryType::UNSPECIFIED;

    // The initial net priority for the host resolution request.
    RequestPriority initial_priority = RequestPriority::DEFAULT_PRIORITY;

    // The source to use for resolved addresses. Default allows the resolver to
    // pick an appropriate source. Only affects use of big external sources (eg
    // calling the system for resolution or using DNS). Even if a source is
    // specified, results can still come from cache, resolving "localhost" or
    // IP literals, etc.
    HostResolverSource source = HostResolverSource::ANY;

    enum class CacheUsage {
      // Results may come from the host cache if non-stale.
      ALLOWED,

      // Results may come from the host cache even if stale (by expiration or
      // network changes). In secure dns AUTOMATIC mode, the cache is checked
      // for both secure and insecure results prior to any secure DNS lookups to
      // minimize response time.
      STALE_ALLOWED,

      // Results will not come from the host cache.
      DISALLOWED,
    };
    CacheUsage cache_usage = CacheUsage::ALLOWED;

    // If |true|, requests special behavior that the "canonical name" be
    // requested from the system and be returned as the only entry in
    // `ResolveHostRequest::GetDnsAliasResults()` results. Setting this
    // parameter is disallowed for any requests that cannot be resolved using
    // the system resolver, e.g. non-address requests or requests specifying a
    // non-`SYSTEM` `source`.
    //
    // TODO(crbug.com/40209534): Consider allowing the built-in resolver to
    // still be used with this parameter. Would then function as a request to
    // just keep the single final name from the alias chain instead of all
    // aliases, and also skip the canonicalization unless that canonicalization
    // is found to be fine for usage.
    bool include_canonical_name = false;

    // Hint to the resolver that resolution is only being requested for loopback
    // hosts.
    bool loopback_only = false;

    // Set |true| iff the host resolve request is only being made speculatively
    // to fill the cache and the result addresses will not be used. The request
    // will receive special logging/observer treatment, and the result addresses
    // will always be |std::nullopt|.
    bool is_speculative = false;

    // If `true`, resolver may (but is not guaranteed to) take steps to avoid
    // the name being resolved via LLMNR or mDNS. Useful for requests where it
    // is not desired to wait for longer timeouts on potential negative results,
    // as is typically the case for LLMNR or mDNS queries without any results.
    bool avoid_multicast_resolution = false;

    // Controls the resolver's Secure DNS behavior for this request.
    SecureDnsPolicy secure_dns_policy = SecureDnsPolicy::kAllow;
  };

  // Handler for an ongoing MDNS listening operation. Created by
  // HostResolver::CreateMdnsListener().
  class MdnsListener {
   public:
    // Delegate type for result update notifications from MdnsListener. All
    // methods have a |result_type| field to allow a single delegate to be
    // passed to multiple MdnsListeners and be used to listen for updates for
    // multiple types for the same host.
    class Delegate {
     public:
      virtual ~Delegate() = default;

      virtual void OnAddressResult(MdnsListenerUpdateType update_type,
                                   DnsQueryType result_type,
                                   IPEndPoint address) = 0;
      virtual void OnTextResult(MdnsListenerUpdateType update_type,
                                DnsQueryType result_type,
                                std::vector<std::string> text_records) = 0;
      virtual void OnHostnameResult(MdnsListenerUpdateType update_type,
                                    DnsQueryType result_type,
                                    HostPortPair host) = 0;

      // For results which may be valid MDNS but are not handled/parsed by
      // HostResolver, e.g. pointers to the root domain.
      virtual void OnUnhandledResult(MdnsListenerUpdateType update_type,
                                     DnsQueryType result_type) = 0;
    };

    // Destruction cancels the listening operation.
    virtual ~MdnsListener() = default;

    // Begins the listening operation, invoking |delegate| whenever results are
    // updated. |delegate| will no longer be called once the listening operation
    // is cancelled (via destruction of |this|).
    virtual int Start(Delegate* delegate) = 0;
  };

  HostResolver(const HostResolver&) = delete;
  HostResolver& operator=(const HostResolver&) = delete;

  // If any completion callbacks are pending when the resolver is destroyed,
  // the host resolutions are cancelled, and the completion callbacks will not
  // be called.
  virtual ~HostResolver();

  // Cancels any pending requests without calling callbacks, same as
  // destruction, except also leaves the resolver in a mostly-noop state. Any
  // future request Start() calls (for requests created before or after
  // OnShutdown()) will immediately fail with ERR_CONTEXT_SHUT_DOWN.
  virtual void OnShutdown() = 0;

  // Creates a request to resolve the given hostname (or IP address literal).
  // Profiling information for the request is saved to |net_log| if non-NULL.
  //
  // Additional parameters may be set using |optional_parameters|. Reasonable
  // defaults will be used if passed |nullptr|.
  virtual std::unique_ptr<ResolveHostRequest> CreateRequest(
      url::SchemeHostPort host,
      NetworkAnonymizationKey network_anonymization_key,
      NetLogWithSource net_log,
      std::optional<ResolveHostParameters> optional_parameters) = 0;

  // Create requests when scheme is unknown or non-standard.
  // TODO(crbug.com/40181080): Rename to discourage use when scheme is known.
  virtual std::unique_ptr<ResolveHostRequest> CreateRequest(
      const HostPortPair& host,
      const NetworkAnonymizationKey& network_anonymization_key,
      const NetLogWithSource& net_log,
      const std::optional<ResolveHostParameters>& optional_parameters) = 0;

  // Creates a service endpoint resolution request.
  virtual std::unique_ptr<ServiceEndpointRequest> CreateServiceEndpointRequest(
      Host host,
      NetworkAnonymizationKey network_anonymization_key,
      NetLogWithSource net_log,
      ResolveHostParameters parameters) = 0;

  // Creates a request to probe configured DoH servers to find which can be used
  // successfully.
  virtual std::unique_ptr<ProbeRequest> CreateDohProbeRequest();

  // Create a listener to watch for updates to an MDNS result.
  virtual std::unique_ptr<MdnsListener> CreateMdnsListener(
      const HostPortPair& host,
      DnsQueryType query_type);

  // Returns the HostResolverCache |this| uses, or NULL if there isn't one.
  // Used primarily to clear the cache and for getting debug information.
  virtual HostCache* GetHostCache();

  // Returns the current DNS configuration |this| is using, as a Value.
  virtual base::Value::Dict GetDnsConfigAsValue() const;

  // Set the associated URLRequestContext, generally expected to be called by
  // URLRequestContextBuilder on passing ownership of |this| to a context. May
  // only be called once.
  virtual void SetRequestContext(URLRequestContext* request_context);

  virtual HostResolverManager* GetManagerForTesting();
  virtual const URLRequestContext* GetContextForTesting() const;
  virtual handles::NetworkHandle GetTargetNetworkForTesting() const;

  // Creates a new HostResolver. |manager| must outlive the returned resolver.
  //
  // If |mapping_rules| is non-empty, the mapping rules will be applied to
  // requests.  See MappedHostResolver for details.
  static std::unique_ptr<HostResolver> CreateResolver(
      HostResolverManager* manager,
      std::string_view host_mapping_rules = "",
      bool enable_caching = true);

  // Creates a HostResolver independent of any global HostResolverManager. Only
  // for tests and standalone tools not part of the browser.
  //
  // If |mapping_rules| is non-empty, the mapping rules will be applied to
  // requests.  See MappedHostResolver for details.
  static std::unique_ptr<HostResolver> CreateStandaloneResolver(
      NetLog* net_log,
      std::optional<ManagerOptions> options = std::nullopt,
      std::string_view host_mapping_rules = "",
      bool enable_caching = true);
  // Same, but explicitly returns the implementing ContextHostResolver. Only
  // used by tests and by StaleHostResolver in Cronet. No mapping rules can be
  // applied because doing so requires wrapping the ContextHostResolver.
  static std::unique_ptr<ContextHostResolver> CreateStandaloneContextResolver(
      NetLog* net_log,
      std::optional<ManagerOptions> options = std::nullopt,
      bool enable_caching = true);
  // Same, but bind the resolver to `target_network`: all lookups will be
  // performed exclusively for `target_network`, lookups will fail if
  // `target_network` disconnects. This can only be used by network-bound
  // URLRequestContexts.
  // Due to the current implementation, if `options` is specified, its
  // DnsConfigOverrides parameter must be empty.
  // Only implemented for Android starting from Marshmallow.
  static std::unique_ptr<HostResolver> CreateStandaloneNetworkBoundResolver(
      NetLog* net_log,
      handles::NetworkHandle network,
      std::optional<ManagerOptions> options = std::nullopt,
      std::string_view host_mapping_rules = "",
      bool enable_caching = true);

  // Helpers for interacting with HostCache and ProcResolver.
  static AddressFamily DnsQueryTypeSetToAddressFamily(
      DnsQueryTypeSet query_types);
  static HostResolverFlags ParametersToHostResolverFlags(
      const ResolveHostParameters& parameters);

  // Helper for squashing error code to a small set of DNS error codes.
  static int SquashErrorCode(int error);

  // Builds an AddressList from the first non-protocol endpoint found in
  // `endpoints`.
  //
  // TODO(crbug.com/40203587): Delete once `AddressList` usage is fully replaced
  // in `HostResolver` and results.
  static AddressList EndpointResultToAddressList(
      base::span<const HostResolverEndpointResult> endpoints,
      const std::set<std::string>& aliases);

  // Returns whether there is at least one protocol endpoint in `endpoints`, and
  // all such endpoints have ECH parameters. This can be used to implement the
  // guidance in section 3 of RFC9460.
  template <typename T>
  static bool AllProtocolEndpointsHaveEch(base::span<const T> endpoints)
    requires HasConnectionEndpointMetadata<T>
  {
    bool has_svcb = false;
    for (const auto& endpoint : endpoints) {
      if (!endpoint.metadata.supported_protocol_alpns.empty()) {
        has_svcb = true;
        if (endpoint.metadata.ech_config_list.empty()) {
          return false;  // There is a non-ECH SVCB/HTTPS route.
        }
      }
    }
    // Either there were no SVCB/HTTPS records (should be SVCB-optional), or
    // there were and all supported ECH (should be SVCB-reliant).
    return has_svcb;
  }

  // Returns true if NAT64 can be used in place of an IPv4 address during host
  // resolution.
  static bool MayUseNAT64ForIPv4Literal(HostResolverFlags flags,
                                        HostResolverSource source,
                                        const IPAddress& ip_address);

 protected:
  HostResolver();

  // Utility to create a request implementation that always fails with |error|
  // immediately on start.
  static std::unique_ptr<ResolveHostRequest> CreateFailingRequest(int error);
  static std::unique_ptr<ProbeRequest> CreateFailingProbeRequest(int error);
  static std::unique_ptr<ServiceEndpointRequest>
  CreateFailingServiceEndpointRequest(int error);
};

}  // namespace net

#endif  // NET_DNS_HOST_RESOLVER_H_
