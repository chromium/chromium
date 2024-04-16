// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "components/wifi/wifi_service.h"

namespace wifi {

// TODO(crbug.com/40198322): Implement WifiServiceFuchsia.
class WifiServiceFuchsia : public WiFiService {
 public:
  WifiServiceFuchsia() = default;
  WifiServiceFuchsia(const WifiServiceFuchsia&) = delete;
  WifiServiceFuchsia& operator=(const WifiServiceFuchsia&) = delete;
  ~WifiServiceFuchsia() override = default;

  // WiFiService interface implementation.
  void Initialize(
      scoped_refptr<base::SequencedTaskRunner> task_runner) override {
    NOTIMPLEMENTED_LOG_ONCE();
  }

  void UnInitialize() override { NOTIMPLEMENTED_LOG_ONCE(); }

  void GetProperties(const std::string& network_guid,
                     base::Value::Dict* properties,
                     std::string* error) override {
    *error = kErrorNotImplemented;
    NOTIMPLEMENTED_LOG_ONCE();
  }

  void GetManagedProperties(const std::string& network_guid,
                            base::Value::Dict* managed_properties,
                            std::string* error) override {
    *error = kErrorNotImplemented;
    NOTIMPLEMENTED_LOG_ONCE();
  }

  void GetState(const std::string& network_guid,
                base::Value::Dict* properties,
                std::string* error) override {
    *error = kErrorNotImplemented;
    NOTIMPLEMENTED_LOG_ONCE();
  }

  void SetProperties(const std::string& network_guid,
                     base::Value::Dict properties,
                     std::string* error) override {
    *error = kErrorNotImplemented;
    NOTIMPLEMENTED_LOG_ONCE();
  }

  void CreateNetwork(bool shared,
                     base::Value::Dict properties,
                     std::string* network_guid,
                     std::string* error) override {
    *error = kErrorNotImplemented;
    NOTIMPLEMENTED_LOG_ONCE();
  }

  void GetVisibleNetworks(const std::string& network_type,
                          bool include_details,
                          base::Value::List* network_list) override {
    NOTIMPLEMENTED_LOG_ONCE();
  }

  void RequestNetworkScan() override { NOTIMPLEMENTED_LOG_ONCE(); }

  void StartConnect(const std::string& network_guid,
                    std::string* error) override {
    *error = kErrorNotImplemented;
    NOTIMPLEMENTED_LOG_ONCE();
  }

  void StartDisconnect(const std::string& network_guid,
                       std::string* error) override {
    *error = kErrorNotImplemented;
    NOTIMPLEMENTED_LOG_ONCE();
  }

  void GetKeyFromSystem(const std::string& network_guid,
                        std::string* key_data,
                        std::string* error) override {
    *error = kErrorNotImplemented;
    NOTIMPLEMENTED_LOG_ONCE();
  }

  void SetEventObservers(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      NetworkGuidListCallback networks_changed_observer,
      NetworkGuidListCallback network_list_changed_observer) override {
    NOTIMPLEMENTED_LOG_ONCE();
  }

  void RequestConnectedNetworkUpdate() override { NOTIMPLEMENTED_LOG_ONCE(); }

  void GetConnectedNetworkSSID(std::string* ssid, std::string* error) override {
    *error = kErrorNotImplemented;
    NOTIMPLEMENTED_LOG_ONCE();
  }
};

WiFiService* WiFiService::Create() {
  return new WifiServiceFuchsia();
}

}  // namespace wifi
