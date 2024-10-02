// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_SERVER_PROPERTIES_H_
#define NET_HTTP_HTTP_SERVER_PROPERTIES_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "base/containers/lru_cache.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/net_export.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/privacy_mode.h"
#include "net/http/alternative_service.h"
#include "net/http/broken_alternative_services.h"
#include "net/third_party/quiche/src/quiche/http2/core/spdy_framer.h"  // TODO(willchan): Reconsider this.
#include "net/third_party/quiche/src/quiche/http2/core/spdy_protocol.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_bandwidth.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_server_id.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"
#include "url/scheme_host_port.h"

namespace base {
class Clock;
class TickClock;
}

namespace net {

class HttpServerPropertiesManager;
class IPAddress;
class NetLog;
struct SSLConfig;

struct NET_EXPORT SupportsQuic {
  SupportsQuic() : used_quic(false) {}
  SupportsQuic(bool used_quic, const std::string& address)
      : used_quic(used_quic), address(address) {}

  bool Equals(const SupportsQuic& other) const {
    return used_quic == other.used_quic && address == other.address;
  }

  bool used_quic;
  std::string address;
};

struct NET_EXPORT ServerNetworkStats {
  ServerNetworkStats() : bandwidth_estimate(quic::QuicBandwidth::Zero()) {}

  bool operator==(const ServerNetworkStats& other) const {
    return srtt == other.srtt && bandwidth_estimate == other.bandwidth_estimate;
  }

  bool operator!=(const ServerNetworkStats& other) const {
    return !this->operator==(other);
  }

  base::TimeDelta srtt;
  quic::QuicBandwidth bandwidth_estimate;
};

typedef std::vector<AlternativeService> AlternativeServiceVector;

// Store at most 200 MRU RecentlyBrokenAlternativeServices in memory and disk.
// This ideally would be with the other constants in HttpServerProperties, but
// has to go here instead of prevent a circular dependency.
const int kMaxRecentlyBrokenAlternativeServiceEntries = 200;

// Store at most 5 MRU QUIC servers by default. This is mainly used by cronet.
const int kDefaultMaxQuicServerEntries = 5;

// The interface for setting/retrieving the HTTP server properties.
// Currently, this class manages servers':
// * HTTP/2 support;
// * Alternative Service support;
// * QUIC data (like ServerNetworkStats and QuicServerInfo).
//
// Optionally retrieves and saves properties from/to disk. This class is not
// threadsafe.
class NET_EXPORT HttpServerProperties
    : public BrokenAlternativeServices::Delegate {
 public:
  // Store at most 500 MRU ServerInfos in memory and disk.
  static const int kMaxServerInfoEntries = 500;

  // Provides an interface to interact with persistent preferences storage
  // implemented by the embedder. The prefs are assumed not to have been loaded
  // before HttpServerPropertiesManager construction.
  class NET_EXPORT PrefDelegate {
   public:
    virtual ~PrefDelegate();

    // Returns the branch of the preferences system for the server properties.
    // Returns nullptr if the pref system has no data for the server properties.
    virtual const base::Value::Dict& GetServerProperties() const = 0;

    // Sets the server properties to the given value. If |callback| is
    // non-empty, flushes data to persistent storage and invokes |callback|
    // asynchronously when complete.
    virtual void SetServerProperties(base::Value::Dict dict,
                                     base::OnceClosure callback) = 0;

    // Starts listening for prefs to be loaded. If prefs are already loaded,
    // |pref_loaded_callback| will be invoked asynchronously. Callback will be
    // invoked even if prefs fail to load. Will only be called once by the
    // HttpServerPropertiesManager.
    virtual void WaitForPrefLoad(base::OnceClosure pref_loaded_callback) = 0;
  };

  // Contains metadata about a particular server. Note that all methods that
  // take a "SchemeHostPort" expect schemes of ws and wss to be mapped to http
  // and https, respectively. See GetNormalizedSchemeHostPort().
  struct NET_EXPORT ServerInfo {
    ServerInfo();
    ServerInfo(const ServerInfo& server_info);
    ServerInfo(ServerInfo&& server_info);
    ~ServerInfo();

    // Returns true if no fields are populated.
    bool empty() const;

    // Used in tests.
    bool operator==(const ServerInfo& other) const;

    // IMPORTANT:  When adding a field here, be sure to update
    // HttpServerProperties::OnServerInfoLoaded() as well as
    // HttpServerPropertiesManager to correctly load/save the from/to the pref
    // store.

    // Whether or not a server is known to support H2/SPDY. False indicates
    // known lack of support, true indicates known support, and not set
    // indicates unknown. The difference between false and not set only matters
    // when loading from disk, when an initialized false value will take
    // priority over a not set value.
    std::optional<bool> supports_spdy;

    // True if the server has previously indicated it required HTTP/1.1. Unlike
    // other fields, not persisted to disk.
    std::optional<bool> requires_http11;

    std::optional<AlternativeServiceInfoVector> alternative_services;
    std::optional<ServerNetworkStats> server_network_stats;
  };

  struct NET_EXPORT ServerInfoMapKey {
    // If |use_network_anonymization_key| is false, an empty
    // NetworkAnonymizationKey is used instead of |network_anonymization_key|.
    // Note that |server| can be passed in via std::move(), since most callsites
    // can pass a recently created SchemeHostPort.
    ServerInfoMapKey(url::SchemeHostPort server,
                     const NetworkAnonymizationKey& network_anonymization_key,
                     bool use_network_anonymization_key);
    ~ServerInfoMapKey();

    bool operator<(const ServerInfoMapKey& other) const;

    // IMPORTANT: The constructor normalizes the scheme so that "ws" is replaced
    // by "http" and "wss" by "https", so this should never be compared directly
    // with values passed into to HttpServerProperties methods.
    url::SchemeHostPort server;

    NetworkAnonymizationKey network_anonymization_key;
  };

  class NET_EXPORT ServerInfoMap
      : public base::LRUCache<ServerInfoMapKey, ServerInfo> {
   public:
    ServerInfoMap();

    ServerInfoMap(const ServerInfoMap&) = delete;
    ServerInfoMap& operator=(const ServerInfoMap&) = delete;

    // If there's an entry corresponding to |key|, brings that entry to the
    // front and returns an iterator to it. Otherwise, inserts an empty
    // ServerInfo using |key|, and returns an iterator to it.
    iterator GetOrPut(const ServerInfoMapKey& key);

    // Erases the ServerInfo identified by |server_info_it| if no fields have
    // data. The iterator must point to an entry in the map. Regardless of
    // whether the entry is removed or not, returns iterator for the next entry.
    iterator EraseIfEmpty(iterator server_info_it);
  };

  struct NET_EXPORT QuicServerInfoMapKey {
    // If |use_network_anonymization_key| is false, an empty
    // NetworkAnonymizationKey is used instead of |network_anonymization_key|.
    QuicServerInfoMapKey(
        const quic::QuicServerId& server_id,
        PrivacyMode privacy_mode,
        const NetworkAnonymizationKey& network_anonymization_key,
        bool use_network_anonymization_key);
    ~QuicServerInfoMapKey();

    bool operator<(const QuicServerInfoMapKey& other) const;

    // Used in tests.
    bool operator==(const QuicServerInfoMapKey& other) const;

    quic::QuicServerId server_id;
    PrivacyMode privacy_mode = PRIVACY_MODE_DISABLED;
    NetworkAnonymizationKey network_anonymization_key;
  };

  // Max number of quic servers to store is not hardcoded and can be set.
  // Because of this, QuicServerInfoMap will not be a subclass of LRUCache.
  // Separate from ServerInfoMap because the key includes privacy mode (Since
  // this is analogous to the SSL session cache, which has separate caches for
  // privacy mode), and each entry can be quite large, so it has its own size
  // limit, which is much smaller than the ServerInfoMap's limit.
  typedef base::LRUCache<QuicServerInfoMapKey, std::string> QuicServerInfoMap;

  // If a |pref_delegate| is specified, it will be used to read/write the
  // properties to a pref file. Writes are rate limited to improve performance.
  //
  // |tick_clock| is used for setting expiration times and scheduling the
  // expiration of broken alternative services. If null, default clock will be
  // used.
  //
  // |clock| is used for converting base::TimeTicks to base::Time for
  // wherever base::Time is preferable.
  explicit HttpServerProperties(
      std::unique_ptr<PrefDelegate> pref_delegate = nullptr,
      NetLog* net_log = nullptr,
      const base::TickClock* tick_clock = nullptr,
      base::Clock* clock = nullptr);

  HttpServerProperties(const HttpServerProperties&) = delete;
  HttpServerProperties& operator=(const HttpServerProperties&) = delete;

  ~HttpServerProperties() override;

  // Deletes all data. If |callback| is non-null, flushes data to disk
  // and invokes the callback asynchronously once changes have been written to
  // disk.
  void Clear(base::OnceClosure callback);

  // Returns true if |server|, in the context of |network_anonymization_key|,
  // has previously supported a network protocol which honors request
  // prioritization.
  //
  // Note that this also implies that the server supports request
  // multiplexing, since priorities imply a relationship between
  // multiple requests.
  bool SupportsRequestPriority(
      const url::SchemeHostPort& server,
      const NetworkAnonymizationKey& network_anonymization_key);

  // Returns the value set by SetSupportsSpdy(). If not set, returns false.
  bool GetSupportsSpdy(
      const url::SchemeHostPort& server,
      const NetworkAnonymizationKey& network_anonymization_key);

  // Records whether |server| supports H2 or not. Information is restricted to
  // the context of |network_anonymization_key|, to prevent cross-site
  // information leakage.
  void SetSupportsSpdy(const url::SchemeHostPort& server,
                       const NetworkAnonymizationKey& network_anonymization_key,
                       bool supports_spdy);

  // Returns true if |server| has required HTTP/1.1 via HTTP/2 error code, in
  // the context of |network_anonymization_key|.
  //
  // Any relevant HostMappingRules must already have been applied to `server`.
  bool RequiresHTTP11(const url::SchemeHostPort& server,
                      const NetworkAnonymizationKey& network_anonymization_key);

  // Require HTTP/1.1 on subsequent connections, in the context of
  // |network_anonymization_key|.  Not persisted.
  //
  // Any relevant HostMappingRules must already have been applied to `server`.
  void SetHTTP11Required(
      const url::SchemeHostPort& server,
      const NetworkAnonymizationKey& network_anonymization_key);

  // Modify SSLConfig to force HTTP/1.1 if necessary.
  //
  // Any relevant HostMappingRules must already have been applied to `server`.
  void MaybeForceHTTP11(
      const url::SchemeHostPort& server,
      const NetworkAnonymizationKey& network_anonymization_key,
      SSLConfig* ssl_config);

  // Return all alternative services for |origin|, learned in the context of
  // |network_anonymization_key|, including broken ones. Returned alternative
  // services never have empty hostnames.
  AlternativeServiceInfoVector GetAlternativeServiceInfos(
      const url::SchemeHostPort& origin,
      const NetworkAnonymizationKey& network_anonymization_key);

  // Set a single HTTP/2 alternative service for |origin|.  Previous
  // alternative services for |origin| are discarded.
  // |alternative_service.host| may be empty.
  void SetHttp2AlternativeService(
      const url::SchemeHostPort& origin,
      const NetworkAnonymizationKey& network_anonymization_key,
      const AlternativeService& alternative_service,
      base::Time expiration);

  // Set a single QUIC alternative service for |origin|.  Previous alternative
  // services for |origin| are discarded.
  // |alternative_service.host| may be empty.
  void SetQuicAlternativeService(
      const url::SchemeHostPort& origin,
      const NetworkAnonymizationKey& network_anonymization_key,
      const AlternativeService& alternative_service,
      base::Time expiration,
      const quic::ParsedQuicVersionVector& advertised_versions);

  // Set alternative services for |origin|, learned in the context of
  // |network_anonymization_key|.  Previous alternative services for |origin|
  // are discarded. Hostnames in |alternative_service_info_vector| may be empty.
  // |alternative_service_info_vector| may be empty.
  void SetAlternativeServices(
      const url::SchemeHostPort& origin,
      const NetworkAnonymizationKey& network_anonymization_key,
      const AlternativeServiceInfoVector& alternative_service_info_vector);

  // Marks |alternative_service| as broken in the context of
  // |network_anonymization_key|. |alternative_service.host| must not be empty.
  void MarkAlternativeServiceBroken(
      const AlternativeService& alternative_service,
      const NetworkAnonymizationKey& network_anonymization_key);

  // Marks |alternative_service| as broken in the context of
  // |network_anonymization_key| until the default network changes.
  // |alternative_service.host| must not be empty.
  void MarkAlternativeServiceBrokenUntilDefaultNetworkChanges(
      const AlternativeService& alternative_service,
      const NetworkAnonymizationKey& network_anonymization_key);

  // Marks |alternative_service| as recently broken in the context of
  // |network_anonymization_key|. |alternative_service.host| must not be empty.
  void MarkAlternativeServiceRecentlyBroken(
      const AlternativeService& alternative_service,
      const NetworkAnonymizationKey& network_anonymization_key);

  // Returns true iff |alternative_service| is currently broken in the context
  // of |network_anonymization_key|. |alternative_service.host| must not be
  // empty.
  bool IsAlternativeServiceBroken(
      const AlternativeService& alternative_service,
      const NetworkAnonymizationKey& network_anonymization_key) const;

  // Returns true iff |alternative_service| was recently broken in the context
  // of |network_anonymization_key|. |alternative_service.host| must not be
  // empty.
  bool WasAlternativeServiceRecentlyBroken(
      const AlternativeService& alternative_service,
      const NetworkAnonymizationKey& network_anonymization_key);

  // Confirms that |alternative_service| is working in the context of
  // |network_anonymization_key|. |alternative_service.host| must not be empty.
  void ConfirmAlternativeService(
      const AlternativeService& alternative_service,
      const NetworkAnonymizationKey& network_anonymization_key);

  // Called when the default network changes.
  // Clears all the alternative services that were marked broken until the
  // default network changed.
  void OnDefaultNetworkChanged();

  // Returns all alternative service mappings as human readable strings.
  // Empty alternative service hostnames will be printed as such.
  base::Value GetAlternativeServiceInfoAsValue() const;

  // Tracks the last local address when QUIC was known to work. The address
  // cannot be set to an empty address - use
  // ClearLastLocalAddressWhenQuicWorked() if it needs to be cleared.
  bool WasLastLocalAddressWhenQuicWorked(const IPAddress& local_address) const;
  bool HasLastLocalAddressWhenQuicWorked() const;
  void SetLastLocalAddressWhenQuicWorked(
      IPAddress last_local_address_when_quic_worked);
  void ClearLastLocalAddressWhenQuicWorked();

  // Sets |stats| for |server|.
  void SetServerNetworkStats(
      const url::SchemeHostPort& server,
      const NetworkAnonymizationKey& network_anonymization_key,
      ServerNetworkStats stats);

  // Clears any stats for |server|.
  void ClearServerNetworkStats(
      const url::SchemeHostPort& server,
      const NetworkAnonymizationKey& network_anonymization_key);

  // Returns any stats for |server| or nullptr if there are none.
  const ServerNetworkStats* GetServerNetworkStats(
      const url::SchemeHostPort& server,
      const NetworkAnonymizationKey& network_anonymization_key);

  // Save QuicServerInfo (in std::string form) for the given |server_id|, in the
  // context of |network_anonymization_key|.
  void SetQuicServerInfo(
      const quic::QuicServerId& server_id,
      PrivacyMode privacy_mode,
      const NetworkAnonymizationKey& network_anonymization_key,
      const std::string& server_info);

  // Get QuicServerInfo (in std::string form) for the given |server_id|, in the
  // context of |network_anonymization_key|.
  const std::string* GetQuicServerInfo(
      const quic::QuicServerId& server_id,
      PrivacyMode privacy_mode,
      const NetworkAnonymizationKey& network_anonymization_key);

  // Returns all persistent QuicServerInfo objects.
  const QuicServerInfoMap& quic_server_info_map() const;

  // Returns the number of server configs (QuicServerInfo objects) persisted.
  size_t max_server_configs_stored_in_properties() const;

  // Sets the number of server configs (QuicServerInfo objects) to be persisted.
  void SetMaxServerConfigsStoredInProperties(
      size_t max_server_configs_stored_in_properties);

  // If values are present, sets initial_delay and
  // exponential_backoff_on_initial_delay which are used to calculate delay of
  // broken alternative services.
  void SetBrokenAlternativeServicesDelayParams(
      std::optional<base::TimeDelta> initial_delay,
      std::optional<bool> exponential_backoff_on_initial_delay);

  // Returns whether HttpServerProperties is initialized.
  bool IsInitialized() const;

  // BrokenAlternativeServices::Delegate method.
  void OnExpireBrokenAlternativeService(
      const AlternativeService& expired_alternative_service,
      const NetworkAnonymizationKey& network_anonymization_key) override;

  static base::TimeDelta GetUpdatePrefsDelayForTesting();

  // Test-only routines that call the methods used to load the specified
  // field(s) from a prefs file. Unlike OnPrefsLoaded(), these may be invoked
  // multiple times.
  void OnServerInfoLoadedForTesting(
      std::unique_ptr<ServerInfoMap> server_info_map) {
    OnServerInfoLoaded(std::move(server_info_map));
  }
  void OnLastLocalAddressWhenQuicWorkedForTesting(
      const IPAddress& last_local_address_when_quic_worked) {
    OnLastLocalAddressWhenQuicWorkedLoaded(last_local_address_when_quic_worked);
  }
  void OnQuicServerInfoMapLoadedForTesting(
      std::unique_ptr<QuicServerInfoMap> quic_server_info_map) {
    OnQuicServerInfoMapLoaded(std::move(quic_server_info_map));
  }
  void OnBrokenAndRecentlyBrokenAlternativeServicesLoadedForTesting(
      std::unique_ptr<BrokenAlternativeServiceList>
          broken_alternative_service_list,
      std::unique_ptr<RecentlyBrokenAlternativeServices>
          recently_broken_alternative_services) {
    OnBrokenAndRecentlyBrokenAlternativeServicesLoaded(
        std::move(broken_alternative_service_list),
        std::move(recently_broken_alternative_services));
  }

  const std::string* GetCanonicalSuffixForTesting(
      const std::string& host) const {
    return GetCanonicalSuffix(host);
  }

  const ServerInfoMap& server_info_map_for_testing() const {
    return server_info_map_;
  }

  // This will invalidate the start-up properties if called before
  // initialization.
  void FlushWritePropertiesForTesting(base::OnceClosure callback);

  const BrokenAlternativeServices& broken_alternative_services_for_testing()
      const {
    return broken_alternative_services_;
  }

  const QuicServerInfoMap& quic_server_info_map_for_testing() const {
    return quic_server_info_map_;
  }

  // TODO(mmenke): Look into removing this.
  HttpServerPropertiesManager* properties_manager_for_testing() {
    return properties_manager_.get();
  }

 private:
  // TODO (wangyix): modify HttpServerProperties unit tests so this
  // friendness is no longer required.
  friend class HttpServerPropertiesPeer;

  typedef base::flat_map<ServerInfoMapKey, url::SchemeHostPort> CanonicalMap;
  typedef base::flat_map<QuicServerInfoMapKey, quic::QuicServerId>
      QuicCanonicalMap;
  typedef std::vector<std::string> CanonicalSuffixList;

  // Internal implementations of public methods. SchemeHostPort argument must be
  // normalized before calling (ws/wss replaced with http/https). Use wrapped
  // functions instead of putting the normalization in the public functions to
  // reduce chance of regression - normalization in ServerInfoMapKey's
  // constructor would leave |server.scheme| as wrong if not access through the
  // key, and explicit normalization to create |normalized_server| means the one
  // with the incorrect scheme would still be available.
  bool GetSupportsSpdyInternal(
      url::SchemeHostPort server,
      const NetworkAnonymizationKey& network_anonymization_key);
  void SetSupportsSpdyInternal(
      url::SchemeHostPort server,
      const NetworkAnonymizationKey& network_anonymization_key,
      bool supports_spdy);
  bool RequiresHTTP11Internal(
      url::SchemeHostPort server,
      const NetworkAnonymizationKey& network_anonymization_key);
  void SetHTTP11RequiredInternal(
      url::SchemeHostPort server,
      const NetworkAnonymizationKey& network_anonymization_key);
  void MaybeForceHTTP11Internal(
      url::SchemeHostPort server,
      const NetworkAnonymizationKey& network_anonymization_key,
      SSLConfig* ssl_config);
  AlternativeServiceInfoVector GetAlternativeServiceInfosInternal(
      const url::SchemeHostPort& origin,
      const NetworkAnonymizationKey& network_anonymization_key);
  void SetAlternativeServicesInternal(
      const url::SchemeHostPort& origin,
      const NetworkAnonymizationKey& network_anonymization_key,
      const AlternativeServiceInfoVector& alternative_service_info_vector);
  void SetServerNetworkStatsInternal(
      url::SchemeHostPort server,
      const NetworkAnonymizationKey& network_anonymization_key,
      ServerNetworkStats stats);
  void ClearServerNetworkStatsInternal(
      url::SchemeHostPort server,
      const NetworkAnonymizationKey& network_anonymization_key);
  const ServerNetworkStats* GetServerNetworkStatsInternal(
      url::SchemeHostPort server,
      const NetworkAnonymizationKey& network_anonymization_key);

  // Helper functions to use the passed in parameters and
  // |use_network_anonymization_key_| to create a [Quic]ServerInfoMapKey.
  ServerInfoMapKey CreateServerInfoKey(
      const url::SchemeHostPort& server,
      const NetworkAnonymizationKey& network_anonymization_key) const;
  QuicServerInfoMapKey CreateQuicServerInfoKey(
      const quic::QuicServerId& server_id,
      PrivacyMode privacy_mode,
      const NetworkAnonymizationKey& network_anonymization_key) const;

  // Return the iterator for |server| in the context of
  // |network_anonymization_key|, or for its canonical host, or end. Skips over
  // ServerInfos without |alternative_service_info| populated.
  ServerInfoMap::const_iterator GetIteratorWithAlternativeServiceInfo(
      const url::SchemeHostPort& server,
      const NetworkAnonymizationKey& network_anonymization_key);

  // Return the canonical host for |server|  in the context of
  // |network_anonymization_key|, or end if none exists.
  CanonicalMap::const_iterator GetCanonicalAltSvcHost(
      const url::SchemeHostPort& server,
      const NetworkAnonymizationKey& network_anonymization_key) const;

  // Return the canonical host with the same canonical suffix as |server|.
  // The returned canonical host can be used to search for server info in
  // |quic_server_info_map_|. Return 'end' the host doesn't exist.
  QuicCanonicalMap::const_iterator GetCanonicalServerInfoHost(
      const QuicServerInfoMapKey& key) const;

  // Remove the canonical alt-svc host for |server| with
  // |network_anonymization_key|.
  void RemoveAltSvcCanonicalHost(
      const url::SchemeHostPort& server,
      const NetworkAnonymizationKey& network_anonymization_key);

  // Update |canonical_server_info_map_| with the new canonical host.
  // The |key| should have the corresponding server info associated with it
  // in |quic_server_info_map_|. If |canonical_server_info_map_| doesn't
  // have an entry associated with |key|, the method will add one.
  void UpdateCanonicalServerInfoMap(const QuicServerInfoMapKey& key);

  // Returns the canonical host suffix for |host|, or nullptr if none
  // exists.
  const std::string* GetCanonicalSuffix(const std::string& host) const;

  void OnPrefsLoaded(std::unique_ptr<ServerInfoMap> server_info_map,
                     const IPAddress& last_local_address_when_quic_worked,
                     std::unique_ptr<QuicServerInfoMap> quic_server_info_map,
                     std::unique_ptr<BrokenAlternativeServiceList>
                         broken_alternative_service_list,
                     std::unique_ptr<RecentlyBrokenAlternativeServices>
                         recently_broken_alternative_services);

  // These methods are called by OnPrefsLoaded to handle merging properties
  // loaded from prefs with what has been learned while waiting for prefs to
  // load.
  void OnServerInfoLoaded(std::unique_ptr<ServerInfoMap> server_info_map);
  void OnLastLocalAddressWhenQuicWorkedLoaded(
      const IPAddress& last_local_address_when_quic_worked);
  void OnQuicServerInfoMapLoaded(
      std::unique_ptr<QuicServerInfoMap> quic_server_info_map);
  void OnBrokenAndRecentlyBrokenAlternativeServicesLoaded(
      std::unique_ptr<BrokenAlternativeServiceList>
          broken_alternative_service_list,
      std::unique_ptr<RecentlyBrokenAlternativeServices>
          recently_broken_alternative_services);

  // Queue a delayed call to WriteProperties(). If |is_initialized_| is false,
  // or |properties_manager_| is nullptr, or there's already a queued call to
  // WriteProperties(), does nothing.
  void MaybeQueueWriteProperties();

  // Writes cached state to |properties_manager_|, which must not be null.
  // Invokes |callback| on completion, if non-null.
  void WriteProperties(base::OnceClosure callback) const;

  raw_ptr<const base::TickClock> tick_clock_;  // Unowned
  raw_ptr<base::Clock> clock_;                 // Unowned

  // Cached value of whether network state partitioning is enabled. Cached to
  // improve performance.
  const bool use_network_anonymization_key_;

  // Set to true once initial properties have been retrieved from disk by
  // |properties_manager_|. Always true if |properties_manager_| is nullptr.
  bool is_initialized_;

  // Queue a write when resources finish loading. Set to true when
  // MaybeQueueWriteProperties() is invoked while still waiting on
  // initialization to complete.
  bool queue_write_on_load_ = false;

  // Used to load/save properties from/to preferences. May be nullptr.
  std::unique_ptr<HttpServerPropertiesManager> properties_manager_;

  ServerInfoMap server_info_map_;

  BrokenAlternativeServices broken_alternative_services_;

  IPAddress last_local_address_when_quic_worked_;
  // Contains a map of servers which could share the same alternate protocol.
  // Map from a Canonical scheme/host/port/NAK (host is some postfix of host
  // names) to an actual origin, which has a plausible alternate protocol
  // mapping.
  CanonicalMap canonical_alt_svc_map_;

  // Contains list of suffixes (for example ".c.youtube.com",
  // ".googlevideo.com", ".googleusercontent.com") of canonical hostnames.
  const CanonicalSuffixList canonical_suffixes_;

  QuicServerInfoMap quic_server_info_map_;

  // Maps canonical suffixes to host names that have the same canonical suffix
  // and have a corresponding entry in |quic_server_info_map_|. The map can be
  // used to quickly look for server info for hosts that share the same
  // canonical suffix but don't have exact match in |quic_server_info_map_|. The
  // map exists solely to improve the search performance. It only contains
  // derived data that can be recalculated by traversing
  // |quic_server_info_map_|.
  QuicCanonicalMap canonical_server_info_map_;

  size_t max_server_configs_stored_in_properties_;

  // Used to post calls to WriteProperties().
  base::OneShotTimer prefs_update_timer_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_SERVER_PROPERTIES_H_
