// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WIFI_WIFI_SERVICE_H_
#define COMPONENTS_WIFI_WIFI_SERVICE_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "components/wifi/wifi_export.h"

namespace wifi {

// WiFiService interface used by implementation of chrome.networkingPrivate
// JavaScript extension API. All methods should be called on worker thread.
// It could be created on any (including UI) thread, so nothing expensive should
// be done in the constructor. See |NetworkingPrivateService| for wrapper
// accessible on UI thread.
class WIFI_EXPORT WiFiService {
 public:
  using NetworkGuidList = std::vector<std::string>;
  using NetworkGuidListCallback =
      base::RepeatingCallback<void(const NetworkGuidList& network_guid_list)>;

  WiFiService(const WiFiService&) = delete;
  WiFiService& operator=(const WiFiService&) = delete;

  virtual ~WiFiService() = default;

  // Initialize WiFiService, store |task_runner| for posting worker tasks.
  virtual void Initialize(
      scoped_refptr<base::SequencedTaskRunner> task_runner) = 0;

  // UnInitialize WiFiService.
  virtual void UnInitialize() = 0;

  // Create instance of |WiFiService| for normal use.
  static WiFiService* Create();

  // Get Properties of network identified by |network_guid|. Populates
  // |properties| on success, |error| on failure.
  virtual void GetProperties(const std::string& network_guid,
                             base::Value::Dict* properties,
                             std::string* error) = 0;

  // Gets the merged properties of the network with id |network_guid| from the
  // sources: User settings, shared settings, user policy, device policy and
  // the currently active settings. Populates |managed_properties| on success,
  // |error| on failure.
  virtual void GetManagedProperties(const std::string& network_guid,
                                    base::Value::Dict* managed_properties,
                                    std::string* error) = 0;

  // Get the cached read-only properties of the network with id |network_guid|.
  // This is meant to be a higher performance function than |GetProperties|,
  // which requires a round trip to query the networking subsystem. It only
  // returns a subset of the properties returned by |GetProperties|. Populates
  // |properties| on success, |error| on failure.
  virtual void GetState(const std::string& network_guid,
                        base::Value::Dict* properties,
                        std::string* error) = 0;

  // Set Properties of network identified by |network_guid|. Populates |error|
  // on failure.
  virtual void SetProperties(const std::string& network_guid,
                             base::Value::Dict properties,
                             std::string* error) = 0;

  // Creates a new network configuration from |properties|. If |shared| is true,
  // share this network configuration with other users. If a matching configured
  // network already exists, this will fail and populate |error|. On success
  // populates the |network_guid| of the new network.
  virtual void CreateNetwork(bool shared,
                             base::Value::Dict properties,
                             std::string* network_guid,
                             std::string* error) = 0;

  // Get list of visible networks of |network_type| (one of onc::network_type).
  // Populates |network_list| on success.
  virtual void GetVisibleNetworks(const std::string& network_type,
                                  bool include_details,
                                  base::Value::List* network_list) = 0;

  // Request network scan. Send |NetworkListChanged| event on completion.
  virtual void RequestNetworkScan() = 0;

  // Start connect to network identified by |network_guid|. Populates |error|
  // on failure.
  virtual void StartConnect(const std::string& network_guid,
                            std::string* error) = 0;

  // Start disconnect from network identified by |network_guid|. Populates
  // |error| on failure.
  virtual void StartDisconnect(const std::string& network_guid,
                               std::string* error) = 0;

  // Get WiFi Key for network identified by |network_guid| from the
  // system (if it has one) and store it in |key_data|. User privilege elevation
  // may be required, and function will fail if user privileges are not
  // sufficient. Populates |error| on failure.
  virtual void GetKeyFromSystem(const std::string& network_guid,
                                std::string* key_data,
                                std::string* error) = 0;

  // Set observers to run when |NetworksChanged| and |NetworksListChanged|
  // events needs to be sent. Notifications are posted on |task_runner|.
  virtual void SetEventObservers(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      NetworkGuidListCallback networks_changed_observer,
      NetworkGuidListCallback network_list_changed_observer) = 0;

  // Request update of Connected Network information. Send |NetworksChanged|
  // event on completion.
  virtual void RequestConnectedNetworkUpdate() = 0;

  // Get the SSID of the currently connected network, if any.
  virtual void GetConnectedNetworkSSID(std::string* ssid,
                                       std::string* error) = 0;

 protected:
  WiFiService() = default;

  // Error constants.
  static const char kErrorAssociateToNetwork[];
  static const char kErrorInvalidData[];
  static const char kErrorNotConfigured[];
  static const char kErrorNotConnected[];
  static const char kErrorNotFound[];
  static const char kErrorNotImplemented[];
  static const char kErrorScanForNetworksWithName[];
  static const char kErrorWiFiService[];
};

}  // namespace wifi

#endif  // COMPONENTS_WIFI_WIFI_SERVICE_H_
