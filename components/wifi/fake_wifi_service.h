// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WIFI_FAKE_WIFI_SERVICE_H_
#define COMPONENTS_WIFI_FAKE_WIFI_SERVICE_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/task/single_thread_task_runner.h"
#include "components/wifi/network_properties.h"
#include "components/wifi/wifi_service.h"

namespace wifi {

// Fake implementation of WiFiService used to satisfy expectations of
// networkingPrivateApi browser test.
class FakeWiFiService : public WiFiService {
 public:
  FakeWiFiService();

  FakeWiFiService(const FakeWiFiService&) = delete;
  FakeWiFiService& operator=(const FakeWiFiService&) = delete;

  ~FakeWiFiService() override;

  void Initialize(
      scoped_refptr<base::SequencedTaskRunner> task_runner) override;
  void UnInitialize() override;
  void GetProperties(const std::string& network_guid,
                     base::DictionaryValue* properties,
                     std::string* error) override;
  void GetManagedProperties(const std::string& network_guid,
                            base::DictionaryValue* managed_properties,
                            std::string* error) override;
  void GetState(const std::string& network_guid,
                base::DictionaryValue* properties,
                std::string* error) override;
  void SetProperties(const std::string& network_guid,
                     std::unique_ptr<base::DictionaryValue> properties,
                     std::string* error) override;
  void CreateNetwork(bool shared,
                     std::unique_ptr<base::DictionaryValue> properties,
                     std::string* network_guid,
                     std::string* error) override;
  void GetVisibleNetworks(const std::string& network_type,
                          base::ListValue* network_list,
                          bool include_details) override;
  void RequestNetworkScan() override;
  void StartConnect(const std::string& network_guid,
                    std::string* error) override;
  void StartDisconnect(const std::string& network_guid,
                       std::string* error) override;
  void GetKeyFromSystem(const std::string& network_guid,
                        std::string* key_data,
                        std::string* error) override;
  void SetEventObservers(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      NetworkGuidListCallback networks_changed_observer,
      NetworkGuidListCallback network_list_changed_observer) override;
  void RequestConnectedNetworkUpdate() override;
  void GetConnectedNetworkSSID(std::string* ssid,
                               std::string* error) override;

 private:
  NetworkList::iterator FindNetwork(const std::string& network_guid);

  void DisconnectAllNetworksOfType(const std::string& type);

  void SortNetworks();

  void NotifyNetworkListChanged(const NetworkList& networks);

  void NotifyNetworkChanged(const std::string& network_guid);

  NetworkList networks_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  NetworkGuidListCallback networks_changed_observer_;
  NetworkGuidListCallback network_list_changed_observer_;
};

}  // namespace wifi

#endif  // COMPONENTS_WIFI_FAKE_WIFI_SERVICE_H_
