// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_HOST_RESOLVER_H_
#define NET_DNS_HOST_RESOLVER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "net/base/address_family.h"
#include "net/base/completion_once_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/request_priority.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver_source.h"
#include "net/dns/public/dns_config_overrides.h"
#include "net/dns/public/dns_query_type.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/dns/public/secure_dns_mode.h"

namespace base {
class Value;
}

namespace net {

class AddressList;
class ContextHostResolver;
class DnsClient;
struct DnsConfigOverrides;
class HostResolverManager;
class NetLog;
class NetLogWithSource;
class URLRequestContext;

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
  // Handler for an individual host resolution request. Created by
  // HostResolver::CreateRequest().
  class ResolveHostRequest {
   public:
    // Destruction cancels the request if running asynchronously, causing the
    // callback to never be invoked.
    virtual ~ResolveHostRequest() {}

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
    // returning a result other than |ERR_IO_PENDING|.
    virtual const base::Optional<AddressList>& GetAddressResults() const = 0;

    // Text record (TXT) results of the request. Should only be called after
    // Start() signals completion, either by invoking the callback or by
    // returning a result other than |ERR_IO_PENDING|.
    virtual const base::Optional<std::vector<std::string>>& GetTextResults()
        const = 0;

    // Hostname record (SRV or PTR) results of the request. For SRV results,
    // hostnames are ordered acording to their priorities and weights. See RFC
    // 2782.
    //
    // Should only be called after Start() signals completion, either by
    // invoking the callback or by returning a result other than
    // |ERR_IO_PENDING|.
    virtual const base::Optional<std::vector<HostPortPair>>&
    GetHostnameResults() const = 0;

    // INTEGRITY results for an initial experiment related to HTTPSSVC. Each
    // boolean value indicates the intactness of an INTEGRITY record.
    NET_EXPORT virtual const base::Optional<std::vector<bool>>&
    GetIntegrityResultsForTesting() const;

    // Error info for the request.
    //
    // Should only be called after Start() signals completion, either by
    // invoking the callback or by returning a result other than
    // |ERR_IO_PENDING|.
    virtual ResolveErrorInfo GetResolveErrorInfo() const = 0;

    // Information about the result's staleness in the host cache. Only
    // available if results were received from the host cache.
    //
    // Should only be called after Start() signals completion, either by
    // invoking the callback or by returning a result other than
    // |ERR_IO_PENDING|.
    virtual const base::Optional<HostCache::EntryStaleness>& GetStaleInfo()
        const = 0;

    // Changes the priority of the specified request. Can only be called while
    // the request is running (after Start() returns |ERR_IO_PENDING| and before
    // the callback is invoked).
    virtual void ChangeRequestPriority(RequestPriority priority) {}
  };

  // Handler for an activation of probes controlled by a HostResolver. Created
  // by HostResolver::CreateDohProbeRequest().
  class ProbeRequest {
   public:
    // Destruction cancels the request and all probes.
    virtual ~ProbeRequest() {}

    // Activates async running of probes. Always returns ERR_IO_PENDING or an
    // error from activating probes. No callback as probes will never "complete"
    // until cancellation.
    virtual int Start() = 0;
  };

  // Parameter-grouping struct for additional optional parameters for creation
  // of HostResolverManagers and stand-alone HostResolvers.
  struct NET_EXPORT ManagerOptions {
    // Set |max_concurrent_resolves| to this to select a default level
    // of concurrency.
    static const size_t kDefaultParallelism = 0;

    // Set |max_system_retry_attempts| to this to select a default retry value.
    static const size_t kDefaultRetryAttempts;

    // How many resolve requests will be allowed to run in parallel.
    // |kDefaultParallelism| for the resolver to choose a default value.
    size_t max_concurrent_resolves = kDefaultParallelism;

    // The maximum number of times to retry for host resolution if using the
    // system resolver. No effect when the system resolver is not used.
    // |kDefaultRetryAttempts| for the resolver to choose a default value.
    size_t max_system_retry_attempts = kDefaultRetryAttempts;

    // Initial setting for whether the insecure portion of the built-in
    // asynchronous DnsClient is enabled or disabled. See HostResolverManager::
    // SetInsecureDnsClientEnabled() for details.
    bool insecure_dns_client_enabled = false;

    // Initial configuration overrides for the built-in asynchronous DnsClient.
    // See HostResolverManager::SetDnsConfigOverrides() for details.
    DnsConfigOverrides dns_config_overrides;

    // If set to |false|, when on a WiFi connection, IPv6 will be assumed to be
    // unreachable without actually checking. See https://crbug.com/696569 for
    // further context.
    bool check_ipv6_on_wifi = true;
  };

  // Factory class. Useful for classes that need to inject and override resolver
  // creation for tests.
  class NET_EXPORT Factory {
   public:
    virtual ~Factory() = default;

    // See HostResolver::CreateResolver.
    virtual std::unique_ptr<HostResolver> CreateResolver(
        HostResolverManager* manager,
        base::StringPiece host_mapping_rules,
        bool enable_caching);

    // See HostResolver::CreateStandaloneResolver.
    virtual std::unique_ptr<HostResolver> CreateStandaloneResolver(
        NetLog* net_log,
        const ManagerOptions& options,
        base::StringPiece host_mapping_rules,
        bool enable_caching);
  };

  // Parameter-grouping struct for additional optional parameters for
  // CreateRequest() calls. All fields are optional and have a reasonable
  // default.
  struct NET_EXPORT ResolveHostParameters {
    ResolveHostParameters();
    ResolveHostParameters(const ResolveHostParameters& other);

    // Requested DNS query type. If UNSPECIFIED, resolver will pick A or AAAA
    // (or both) based on IPv4/IPv6 settings.
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

    // If |true|, requests that the resolver include AddressList::canonical_name
    // in the results. If the resolver can do so without significant
    // performance impact, canonical_name may still be included even if
    // parameter is set to |false|.
    bool include_canonical_name = false;

    // Hint to the resolver that resolution is only being requested for loopback
    // hosts.
    bool loopback_only = false;

    // Set |true| iff the host resolve request is only being made speculatively
    // to fill the cache and the result addresses will not be used. The request
    // will receive special logging/observer treatment, and the result addresses
    // will always be |base::nullopt|.
    bool is_speculative = false;

    // Set to override the resolver's default secure dns mode for this request.
    base::Optional<SecureDnsMode> secure_dns_mode_override = base::nullopt;
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
      enum class UpdateType { ADDED, CHANGED, REMOVED };

      virtual ~Delegate() {}

      virtual void OnAddressResult(UpdateType update_type,
                                   DnsQueryType result_type,
                                   IPEndPoint address) = 0;
      virtual void OnTextResult(UpdateType update_type,
                                DnsQueryType result_type,
                                std::vector<std::string> text_records) = 0;
      virtual void OnHostnameResult(UpdateType update_type,
                                    DnsQueryType result_type,
                                    HostPortPair host) = 0;

      // For results which may be valid MDNS but are not handled/parsed by
      // HostResolver, e.g. pointers to the root domain.
      virtual void OnUnhandledResult(UpdateType update_type,
                                     DnsQueryType result_type) = 0;
    };

    // Destruction cancels the listening operation.
    virtual ~MdnsListener() {}

    // Begins the listening operation, invoking |delegate| whenever results are
    // updated. |delegate| will no longer be called once the listening operation
    // is cancelled (via destruction of |this|).
    virtual int Start(Delegate* delegate) = 0;
  };

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
  // defaults will be used if passed |base::nullopt|.
  virtual std::unique_ptr<ResolveHostRequest> CreateRequest(
      const HostPortPair& host,
      const NetworkIsolationKey& network_isolation_key,
      const NetLogWithSource& net_log,
      const base::Optional<ResolveHostParameters>& optional_parameters) = 0;

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
  virtual base::Value GetDnsConfigAsValue() const;

  // Set the associated URLRequestContext, generally expected to be called by
  // URLRequestContextBuilder on passing ownership of |this| to a context. May
  // only be called once.
  virtual void SetRequestContext(URLRequestContext* request_context);

  virtual HostResolverManager* GetManagerForTesting();
  virtual const URLRequestContext* GetContextForTesting() const;

  // Creates a new HostResolver. |manager| must outlive the returned resolver.
  //
  // If |mapping_rules| is non-empty, the mapping rules will be applied to
  // requests.  See MappedHostResolver for details.
  static std::unique_ptr<HostResolver> CreateResolver(
      HostResolverManager* manager,
      base::StringPiece host_mapping_rules = "",
      bool enable_caching = true);

  // Creates a HostResolver independent of any global HostResolverManager. Only
  // for tests and standalone tools not part of the browser.
  //
  // If |mapping_rules| is non-empty, the mapping rules will be applied to
  // requests.  See MappedHostResolver for details.
  static std::unique_ptr<HostResolver> CreateStandaloneResolver(
      NetLog* net_log,
      base::Optional<ManagerOptions> options = base::nullopt,
      base::StringPiece host_mapping_rules = "",
      bool enable_caching = true);
  // Same, but explicitly returns the implementing ContextHostResolver. Only
  // used by tests and by StaleHostResolver in Cronet. No mapping rules can be
  // applied because doing so requires wrapping the ContextHostResolver.
  static std::unique_ptr<ContextHostResolver> CreateStandaloneContextResolver(
      NetLog* net_log,
      base::Optional<ManagerOptions> options = base::nullopt,
      bool enable_caching = true);

  // Helpers for interacting with HostCache and ProcResolver.
  static AddressFamily DnsQueryTypeToAddressFamily(DnsQueryType query_type);
  static HostResolverFlags ParametersToHostResolverFlags(
      const ResolveHostParameters& parameters);

  // Helper for squashing error code to a small set of DNS error codes.
  static int SquashErrorCode(int error);

 protected:
  HostResolver();

  // Utility to create a request implementation that always fails with |error|
  // immediately on start.
  static std::unique_ptr<ResolveHostRequest> CreateFailingRequest(int error);
  static std::unique_ptr<ProbeRequest> CreateFailingProbeRequest(int error);

 private:
  DISALLOW_COPY_AND_ASSIGN(HostResolver);
};

}  // namespace net

#endif  // NET_DNS_HOST_RESOLVER_H_
