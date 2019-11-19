// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_SERVER_PROPERTIES_MANAGER_H_
#define NET_HTTP_HTTP_SERVER_PROPERTIES_MANAGER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"
#include "net/http/alternative_service.h"
#include "net/http/broken_alternative_services.h"
#include "net/http/http_server_properties.h"
#include "net/log/net_log_with_source.h"

namespace base {
class DictionaryValue;
class TickClock;
}

namespace net {

class IPAddress;

////////////////////////////////////////////////////////////////////////////////
// HttpServerPropertiesManager

// Class responsible for serializing/deserializing HttpServerProperties and
// reading from/writing to preferences.
class NET_EXPORT_PRIVATE HttpServerPropertiesManager {
 public:
  // Called when prefs are loaded. If prefs completely failed to load,
  // everything will be nullptr. Otherwise, everything will be populated, except
  // |broken_alternative_service_list| and
  // |recently_broken_alternative_services|, which may be null.
  using OnPrefsLoadedCallback = base::OnceCallback<void(
      std::unique_ptr<HttpServerProperties::ServerInfoMap> server_info_map,
      const IPAddress& last_local_address_when_quic_worked,
      std::unique_ptr<HttpServerProperties::QuicServerInfoMap>
          quic_server_info_map,
      std::unique_ptr<BrokenAlternativeServiceList>
          broken_alternative_service_list,
      std::unique_ptr<RecentlyBrokenAlternativeServices>
          recently_broken_alternative_services)>;

  using GetCannonicalSuffix =
      base::RepeatingCallback<const std::string*(const std::string& host)>;

  // Create an instance of the HttpServerPropertiesManager.
  //
  // |on_prefs_loaded_callback| will be invoked with values loaded from
  // |prefs_delegate| once prefs have been loaded from disk.
  // If WriteToPrefs() is invoked before this happens,
  // |on_prefs_loaded_callback| will never be invoked, since the written prefs
  // take precedence over the ones previously stored on disk.
  //
  // |clock| is used for setting expiration times and scheduling the
  // expiration of broken alternative services, and must not be nullptr.
  HttpServerPropertiesManager(
      std::unique_ptr<HttpServerProperties::PrefDelegate> pref_delegate,
      OnPrefsLoadedCallback on_prefs_loaded_callback,
      size_t max_server_configs_stored_in_properties,
      NetLog* net_log,
      const base::TickClock* clock = nullptr);

  ~HttpServerPropertiesManager();

  // Populates passed in objects with data from preferences. If pref data is not
  // present, leaves all values alone. Otherwise, populates them all, with the
  // possible exception of the two broken alt services lists.
  //
  // Corrupted data is ignored.
  //
  // TODO(mmenke): Consider always populating fields, unconditionally, for a
  // simpler API.
  void ReadPrefs(
      std::unique_ptr<HttpServerProperties::ServerInfoMap>* server_info_map,
      IPAddress* last_local_address_when_quic_worked,
      std::unique_ptr<HttpServerProperties::QuicServerInfoMap>*
          quic_server_info_map,
      std::unique_ptr<BrokenAlternativeServiceList>*
          broken_alternative_service_list,
      std::unique_ptr<RecentlyBrokenAlternativeServices>*
          recently_broken_alternative_services);

  void set_max_server_configs_stored_in_properties(
      size_t max_server_configs_stored_in_properties) {
    max_server_configs_stored_in_properties_ =
        max_server_configs_stored_in_properties;
  }

  // Update preferences with caller-provided data. Invokes |callback| when
  // changes have been committed, if non-null.
  //
  // If the OnPrefLoadCallback() passed to the constructor hasn't been invoked
  // by the time this method is called, calling this will prevent it from ever
  // being invoked, as this method will overwrite any previous preferences.
  //
  // Entries associated with NetworkIsolationKeys for opaque origins are not
  // written to disk.
  void WriteToPrefs(
      const HttpServerProperties::ServerInfoMap& server_info_map,
      const GetCannonicalSuffix& get_canonical_suffix,
      const IPAddress& last_local_address_when_quic_worked,
      const HttpServerProperties::QuicServerInfoMap& quic_server_info_map,
      const BrokenAlternativeServiceList& broken_alternative_service_list,
      const RecentlyBrokenAlternativeServices&
          recently_broken_alternative_services,
      base::OnceClosure callback);

 private:
  // TODO(mmenke): Remove these friend methods, and make all methods static that
  // can be.
  FRIEND_TEST_ALL_PREFIXES(HttpServerPropertiesManagerTest,
                           ParseAlternativeServiceInfo);
  FRIEND_TEST_ALL_PREFIXES(HttpServerPropertiesManagerTest,
                           ReadAdvertisedVersionsFromPref);
  FRIEND_TEST_ALL_PREFIXES(HttpServerPropertiesManagerTest,
                           DoNotLoadAltSvcForInsecureOrigins);
  FRIEND_TEST_ALL_PREFIXES(HttpServerPropertiesManagerTest,
                           DoNotLoadExpiredAlternativeService);

  void AddServerData(const base::DictionaryValue& server_dict,
                     HttpServerProperties::ServerInfoMap* server_info_map,
                     bool use_network_isolation_key);

  // Helper method used for parsing an alternative service from JSON.
  // |dict| is the JSON dictionary to be parsed. It should contain fields
  // corresponding to members of AlternativeService.
  // |host_optional| determines whether or not the "host" field is optional. If
  // optional, the default value is empty string.
  // |parsing_under| is used only for debug log outputs in case of error; it
  // should describe what section of the JSON prefs is currently being parsed.
  // |alternative_service| is the output of parsing |dict|.
  // Return value is true if parsing is successful.
  static bool ParseAlternativeServiceDict(
      const base::DictionaryValue& dict,
      bool host_optional,
      const std::string& parsing_under,
      AlternativeService* alternative_service);

  static bool ParseAlternativeServiceInfoDictOfServer(
      const base::DictionaryValue& dict,
      const std::string& server_str,
      AlternativeServiceInfo* alternative_service_info);

  // Attempts to populate |server_info|'s |alternative_service_info| field from
  // |server_dict|. Returns true if the data was no corrupted (Lack of data is
  // not considered corruption).
  static bool ParseAlternativeServiceInfo(
      const url::SchemeHostPort& server,
      const base::DictionaryValue& server_dict,
      HttpServerProperties::ServerInfo* server_info);

  void ReadLastLocalAddressWhenQuicWorked(
      const base::DictionaryValue& server_dict,
      IPAddress* last_local_address_when_quic_worked);
  void ParseNetworkStats(const url::SchemeHostPort& server,
                         const base::DictionaryValue& server_dict,
                         HttpServerProperties::ServerInfo* server_info);
  void AddToQuicServerInfoMap(
      const base::DictionaryValue& server_dict,
      bool use_network_isolation_key,
      HttpServerProperties::QuicServerInfoMap* quic_server_info_map);
  void AddToBrokenAlternativeServices(
      const base::DictionaryValue& broken_alt_svc_entry_dict,
      bool use_network_isolation_key,
      BrokenAlternativeServiceList* broken_alternative_service_list,
      RecentlyBrokenAlternativeServices* recently_broken_alternative_services);

  void SaveAlternativeServiceToServerPrefs(
      const AlternativeServiceInfoVector& alternative_service_info_vector,
      base::DictionaryValue* server_pref_dict);
  void SaveLastLocalAddressWhenQuicWorkedToPrefs(
      const IPAddress& last_local_address_when_quic_worked,
      base::DictionaryValue* http_server_properties_dict);
  void SaveNetworkStatsToServerPrefs(
      const ServerNetworkStats& server_network_stats,
      base::DictionaryValue* server_pref_dict);
  void SaveQuicServerInfoMapToServerPrefs(
      const HttpServerProperties::QuicServerInfoMap& quic_server_info_map,
      base::DictionaryValue* http_server_properties_dict);
  void SaveBrokenAlternativeServicesToPrefs(
      const BrokenAlternativeServiceList& broken_alternative_service_list,
      size_t max_broken_alternative_services,
      const RecentlyBrokenAlternativeServices&
          recently_broken_alternative_services,
      base::DictionaryValue* http_server_properties_dict);

  void OnHttpServerPropertiesLoaded();

  std::unique_ptr<HttpServerProperties::PrefDelegate> pref_delegate_;

  OnPrefsLoadedCallback on_prefs_loaded_callback_;

  size_t max_server_configs_stored_in_properties_;

  const base::TickClock* clock_;  // Unowned

  const NetLogWithSource net_log_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<HttpServerPropertiesManager> pref_load_weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(HttpServerPropertiesManager);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_SERVER_PROPERTIES_MANAGER_H_
