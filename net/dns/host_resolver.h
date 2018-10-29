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

#include "base/optional.h"
#include "net/base/address_family.h"
#include "net/base/completion_once_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/prioritized_dispatcher.h"
#include "net/base/request_priority.h"
#include "net/dns/dns_config.h"
#include "net/dns/host_cache.h"
#include "net/dns/host_resolver_source.h"

namespace base {
class Value;
}

namespace net {

class AddressList;
class DnsClient;
struct DnsConfigOverrides;
class HostResolverImpl;
class NetLog;
class NetLogWithSource;
class URLRequestContext;

// This class represents the task of resolving hostnames (or IP address
// literal) to an AddressList object.
//
// HostResolver can handle multiple requests at a time, so when cancelling a
// request the RequestHandle that was returned by Resolve() needs to be
// given.  A simpler alternative for consumers that only have 1 outstanding
// request at a time is to create a SingleRequestHostResolver wrapper around
// HostResolver (which will automatically cancel the single request when it
// goes out of scope).
class NET_EXPORT HostResolver {
 public:
  // HostResolver::Request class is used to cancel the request and change it's
  // priority. It must be owned by consumer. Deletion cancels the request.
  //
  // TODO(crbug.com/821021): Delete this class once all usage has been
  // converted to the new CreateRequest() API.
  class Request {
   public:
    virtual ~Request() {}

    // Changes the priority of the specified request. Can be called after
    // Resolve() is called. Can't be called once the request is cancelled or
    // completed.
    virtual void ChangeRequestPriority(RequestPriority priority) = 0;
  };

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
    // Results in ERR_NAME_NOT_RESOLVED if the hostname is invalid, or if it is
    // an incompatible IP literal (e.g. IPv6 is disabled and it is an IPv6
    // literal).
    //
    // The parent HostResolver must still be alive when Start() is called,  but
    // if it is destroyed before an asynchronous result completes, the request
    // will be automatically cancelled.
    //
    // If cancelled before |callback| is invoked, it will never be invoked.
    virtual int Start(CompletionOnceCallback callback) = 0;

    // Result of the request. Should only be called after Start() signals
    // completion, either by invoking the callback or by returning a result
    // other than |ERR_IO_PENDING|.
    //
    // TODO(crbug.com/821021): Implement other GetResults() methods for requests
    // that return other data (eg DNS TXT requests).
    virtual const base::Optional<AddressList>& GetAddressResults() const = 0;
  };

  // |max_concurrent_resolves| is how many resolve requests will be allowed to
  // run in parallel. Pass HostResolver::kDefaultParallelism to choose a
  // default value.
  // |max_retry_attempts| is the maximum number of times we will retry for host
  // resolution. Pass HostResolver::kDefaultRetryAttempts to choose a default
  // value.
  // |enable_caching| controls whether a HostCache is used.
  struct NET_EXPORT Options {
    Options();

    PrioritizedDispatcher::Limits GetDispatcherLimits() const;

    size_t max_concurrent_resolves;
    size_t max_retry_attempts;
    bool enable_caching;
  };

  // Factory class. Useful for classes that need to inject and override resolver
  // creation for tests.
  class NET_EXPORT Factory {
   public:
    virtual ~Factory() = default;

    // See HostResolver::CreateSystemResolver.
    virtual std::unique_ptr<HostResolver> CreateResolver(const Options& options,
                                                         NetLog* net_log);
  };

  // The parameters for doing a Resolve(). A hostname and port are
  // required; the rest are optional (and have reasonable defaults).
  //
  // TODO(crbug.com/821021): Delete this class once all usage has been
  // converted to the new CreateRequest() API.
  class NET_EXPORT RequestInfo {
   public:
    explicit RequestInfo(const HostPortPair& host_port_pair);
    RequestInfo(const RequestInfo& request_info);
    ~RequestInfo();

    const HostPortPair& host_port_pair() const { return host_port_pair_; }
    void set_host_port_pair(const HostPortPair& host_port_pair) {
      host_port_pair_ = host_port_pair;
    }

    uint16_t port() const { return host_port_pair_.port(); }
    const std::string& hostname() const { return host_port_pair_.host(); }

    AddressFamily address_family() const { return address_family_; }
    void set_address_family(AddressFamily address_family) {
      address_family_ = address_family;
    }

    HostResolverFlags host_resolver_flags() const {
      return host_resolver_flags_;
    }
    void set_host_resolver_flags(HostResolverFlags host_resolver_flags) {
      host_resolver_flags_ = host_resolver_flags;
    }

    bool allow_cached_response() const { return allow_cached_response_; }
    void set_allow_cached_response(bool b) { allow_cached_response_ = b; }

    bool is_speculative() const { return is_speculative_; }
    void set_is_speculative(bool b) { is_speculative_ = b; }

    bool is_my_ip_address() const { return is_my_ip_address_; }
    void set_is_my_ip_address(bool b) { is_my_ip_address_ = b; }

   private:
    RequestInfo();

    // The hostname to resolve, and the port to use in resulting sockaddrs.
    HostPortPair host_port_pair_;

    // The address family to restrict results to.
    AddressFamily address_family_;

    // Flags to use when resolving this request.
    HostResolverFlags host_resolver_flags_;

    // Whether it is ok to return a result from the host cache.
    bool allow_cached_response_;

    // Whether this request was started by the DNS prefetcher.
    bool is_speculative_;

    // Indicates a request for myIpAddress (to differentiate from other requests
    // for localhost, currently used by Chrome OS).
    bool is_my_ip_address_;
  };

  // DNS query type for a ResolveHostRequest.
  // See:
  // https://www.iana.org/assignments/dns-parameters/dns-parameters.xhtml#dns-parameters-4
  //
  // TODO(crbug.com/846423): Add support for non-address types.
  enum class DnsQueryType {
    UNSPECIFIED,
    A,
    AAAA,
  };

  // Parameter-grouping struct for additional optional parameters for
  // CreateRequest() calls. All fields are optional and have a reasonable
  // default.
  struct ResolveHostParameters {
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

    // If |false|, results will not come from the host cache.
    bool allow_cached_response = true;

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
  };

  // Set Options.max_concurrent_resolves to this to select a default level
  // of concurrency.
  static const size_t kDefaultParallelism = 0;

  // Set Options.max_retry_attempts to this to select a default retry value.
  static const size_t kDefaultRetryAttempts = static_cast<size_t>(-1);

  // If any completion callbacks are pending when the resolver is destroyed,
  // the host resolutions are cancelled, and the completion callbacks will not
  // be called.
  virtual ~HostResolver();

  // Creates a request to resolve the given hostname (or IP address literal).
  // Profiling information for the request is saved to |net_log| if non-NULL.
  //
  // Additional parameters may be set using |optional_parameters|. Reasonable
  // defaults will be used if passed |base::nullopt|.
  //
  // This method is intended as a direct replacement for the old Resolve()
  // method, but it may not yet cover all the capabilities of the old method.
  //
  // TODO(crbug.com/821021): Implement more complex functionality to meet
  // capabilities of Resolve() and M/DnsClient functionality.
  virtual std::unique_ptr<ResolveHostRequest> CreateRequest(
      const HostPortPair& host,
      const NetLogWithSource& net_log,
      const base::Optional<ResolveHostParameters>& optional_parameters) = 0;

  // DEPRECATION NOTE: This method is being replaced by CreateRequest(). New
  // callers should prefer CreateRequest() if it works for their needs.
  //
  // Resolves the given hostname (or IP address literal), filling out the
  // |addresses| object upon success.  The |info.port| parameter will be set as
  // the sin(6)_port field of the sockaddr_in{6} struct.  Returns OK if
  // successful or an error code upon failure.  Returns
  // ERR_NAME_NOT_RESOLVED if hostname is invalid, or if it is an
  // incompatible IP literal (e.g. IPv6 is disabled and it is an IPv6
  // literal).
  //
  // If the operation cannot be completed synchronously, ERR_IO_PENDING will
  // be returned and the real result code will be passed to the completion
  // callback.  Otherwise the result code is returned immediately from this
  // call.
  //
  // [out_req] must be owned by a caller. If the request is not completed
  // synchronously, it will be filled with a handle to the request. It must be
  // completed before the HostResolver itself is destroyed.
  //
  // Requests can be cancelled any time by deletion of the [out_req]. Deleting
  // |out_req| will cancel the request, and cause |callback| not to be invoked.
  //
  // Profiling information for the request is saved to |net_log| if non-NULL.
  //
  // TODO(crbug.com/821021): Delete this method once all usage has been
  // converted to ResolveHost().
  virtual int Resolve(const RequestInfo& info,
                      RequestPriority priority,
                      AddressList* addresses,
                      CompletionOnceCallback callback,
                      std::unique_ptr<Request>* out_req,
                      const NetLogWithSource& net_log) = 0;

  // Resolves the given hostname (or IP address literal) out of cache or HOSTS
  // file (if enabled) only. This is guaranteed to complete synchronously.
  // This acts like |Resolve()| if the hostname is IP literal, or cached value
  // or HOSTS entry exists. Otherwise, ERR_DNS_CACHE_MISS is returned.
  virtual int ResolveFromCache(const RequestInfo& info,
                               AddressList* addresses,
                               const NetLogWithSource& net_log) = 0;

  // Like |ResolveFromCache()|, but can return a stale result if the
  // implementation supports it. Fills in |*stale_info| if a response is
  // returned to indicate how stale (or not) it is.
  virtual int ResolveStaleFromCache(const RequestInfo& info,
                                    AddressList* addresses,
                                    HostCache::EntryStaleness* stale_info,
                                    const NetLogWithSource& source_net_log) = 0;

  // Enable or disable the built-in asynchronous DnsClient.
  virtual void SetDnsClientEnabled(bool enabled);

  // Returns the HostResolverCache |this| uses, or NULL if there isn't one.
  // Used primarily to clear the cache and for getting debug information.
  virtual HostCache* GetHostCache();

  // Checks whether this HostResolver has cached a resolution for the given
  // hostname (or IP address literal). If so, returns true and writes the source
  // of the resolution (e.g. DNS, HOSTS file, etc.) to |source_out| and the
  // staleness of the resolution to |stale_out| (if they are not null).
  // It tries using two common address_family and host_resolver_flag
  // combinations when checking the cache; this means false negatives are
  // possible, but unlikely.
  virtual bool HasCached(base::StringPiece hostname,
                         HostCache::Entry::Source* source_out,
                         HostCache::EntryStaleness* stale_out) const = 0;

  // Returns the current DNS configuration |this| is using, as a Value, or
  // nullptr if it's configured to always use the system host resolver.
  virtual std::unique_ptr<base::Value> GetDnsConfigAsValue() const;

  // Sets the HostResolver to assume that IPv6 is unreachable when on a wifi
  // connection. See https://crbug.com/696569 for further context.
  virtual void SetNoIPv6OnWifi(bool no_ipv6_on_wifi);
  virtual bool GetNoIPv6OnWifi();

  // Sets overriding configuration that will replace or add to configuration
  // read from the system for DnsClient resolution.
  virtual void SetDnsConfigOverrides(const DnsConfigOverrides& overrides);

  virtual void SetRequestContext(URLRequestContext* request_context) {}

  // Returns the currently configured DNS over HTTPS servers. Returns nullptr if
  // DNS over HTTPS is not enabled.
  virtual const std::vector<DnsConfig::DnsOverHttpsServerConfig>*
  GetDnsOverHttpsServersForTesting() const;

  // Creates a HostResolver implementation that queries the underlying system.
  // (Except if a unit-test has changed the global HostResolverProc using
  // ScopedHostResolverProc to intercept requests to the system).
  static std::unique_ptr<HostResolver> CreateSystemResolver(
      const Options& options,
      NetLog* net_log);
  // Same, but explicitly returns the HostResolverImpl. Only used by
  // StaleHostResolver in cronet.
  static std::unique_ptr<HostResolverImpl> CreateSystemResolverImpl(
      const Options& options,
      NetLog* net_log);

  // As above, but uses default parameters.
  static std::unique_ptr<HostResolver> CreateDefaultResolver(NetLog* net_log);
  // Same, but explicitly returns the HostResolverImpl. Only used by
  // StaleHostResolver in cronet.
  static std::unique_ptr<HostResolverImpl> CreateDefaultResolverImpl(
      NetLog* net_log);

  static AddressFamily DnsQueryTypeToAddressFamily(DnsQueryType query_type);

  // Helpers for converting old Resolve() API parameters to new CreateRequest()
  // parameters.
  //
  // TODO(crbug.com/821021): Delete these methods once all usage has been
  // converted to the new CreateRequest() API.
  static DnsQueryType AddressFamilyToDnsQueryType(AddressFamily address_family);
  static ResolveHostParameters RequestInfoToResolveHostParameters(
      const RequestInfo& request_info,
      RequestPriority priority);
  static HostResolverSource FlagsToSource(HostResolverFlags flags);
  static HostResolverFlags ParametersToHostResolverFlags(
      const ResolveHostParameters& parameters);

 protected:
  HostResolver();

 private:
  DISALLOW_COPY_AND_ASSIGN(HostResolver);
};

}  // namespace net

#endif  // NET_DNS_HOST_RESOLVER_H_
