// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wifi/wifi_service.h"

#import <CoreWLAN/CoreWLAN.h>
#import <netinet/in.h>
#import <SystemConfiguration/SystemConfiguration.h>

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/functional/bind.h"
#include "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/single_thread_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "components/onc/onc_constants.h"
#include "components/wifi/network_properties.h"
#include "crypto/apple_keychain.h"

namespace wifi {

// Implementation of WiFiService for macOS.
class WiFiServiceMac : public WiFiService {
 public:
  WiFiServiceMac();

  WiFiServiceMac(const WiFiServiceMac&) = delete;
  WiFiServiceMac& operator=(const WiFiServiceMac&) = delete;

  ~WiFiServiceMac() override;

  // WiFiService interface implementation.
  void Initialize(
      scoped_refptr<base::SequencedTaskRunner> task_runner) override;

  void UnInitialize() override;

  void GetProperties(const std::string& network_guid,
                     base::Value::Dict* properties,
                     std::string* error) override;

  void GetManagedProperties(const std::string& network_guid,
                            base::Value::Dict* managed_properties,
                            std::string* error) override;

  void GetState(const std::string& network_guid,
                base::Value::Dict* properties,
                std::string* error) override;

  void SetProperties(const std::string& network_guid,
                     base::Value::Dict properties,
                     std::string* error) override;

  void CreateNetwork(bool shared,
                     base::Value::Dict properties,
                     std::string* network_guid,
                     std::string* error) override;

  void GetVisibleNetworks(const std::string& network_type,
                          bool include_details,
                          base::Value::List* network_list) override;

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

  void GetConnectedNetworkSSID(std::string* ssid, std::string* error) override;

 private:
  // Checks |ns_error| and if is not |nil|, then stores |error_name|
  // into |error|.
  bool CheckError(NSError* ns_error,
                  const char* error_name,
                  std::string* error) const;

  // Gets |ssid| from unique |network_guid|.
  NSString* SSIDFromGUID(const std::string& network_guid) const {
    return base::SysUTF8ToNSString(network_guid);
  }

  // Gets unique |network_guid| string based on |ssid|.
  std::string GUIDFromSSID(NSString* ssid) const {
    return base::SysNSStringToUTF8(ssid);
  }

  // Populates |properties| from |network|.
  void NetworkPropertiesFromCWNetwork(const CWNetwork* network,
                                      NetworkProperties* properties) const;

  // Returns onc::wifi::k{WPA|WEP}* security constant supported by the
  // |CWNetwork|.
  std::string SecurityFromCWNetwork(const CWNetwork* network) const;

  // Converts |CWChannelBand| into Frequency constant.
  Frequency FrequencyFromCWChannelBand(CWChannelBand band) const;

  // Gets current |onc::connection_state| for given |network_guid|.
  std::string GetNetworkConnectionState(const std::string& network_guid) const;

  // Updates |networks_| with the list of visible wireless networks.
  void UpdateNetworks();

  // Find network by |network_guid| and return iterator to its entry in
  // |networks_|.
  NetworkList::iterator FindNetwork(const std::string& network_guid);

  // Handles notification from |wlan_observer_|.
  void OnWlanObserverNotification();

  // Notifies |network_list_changed_observer_| that list of visible networks has
  // changed to |networks|.
  void NotifyNetworkListChanged(const NetworkList& networks);

  // Notifies |networks_changed_observer_| that network |network_guid|
  // connection state has changed.
  void NotifyNetworkChanged(const std::string& network_guid);

  // Default interface.
  CWInterface* __strong interface_;

  // WLAN Notifications observer.
  id __strong wlan_observer_;

  // Observer to get notified when network(s) have changed (e.g. connect).
  NetworkGuidListCallback networks_changed_observer_;
  // Observer to get notified when network list has changed.
  NetworkGuidListCallback network_list_changed_observer_;
  // Task runner to which events should be posted.
  scoped_refptr<base::SingleThreadTaskRunner> event_task_runner_;
  // Task runner for worker tasks.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  // Cached list of visible networks. Updated by |UpdateNetworks|.
  NetworkList networks_;
  // Guid of last known connected network.
  std::string connected_network_guid_;
  // Temporary storage of network properties indexed by |network_guid|.
  base::Value::Dict network_properties_;
};

WiFiServiceMac::WiFiServiceMac() = default;

WiFiServiceMac::~WiFiServiceMac() {
  UnInitialize();
}

void WiFiServiceMac::Initialize(
  scoped_refptr<base::SequencedTaskRunner> task_runner) {
  task_runner_.swap(task_runner);
  interface_ = [[CWWiFiClient sharedWiFiClient] interface];
  if (!interface_) {
    DVLOG(1) << "Failed to initialize default interface.";
    return;
  }
}

void WiFiServiceMac::UnInitialize() {
  if (wlan_observer_) {
    [NSNotificationCenter.defaultCenter removeObserver:wlan_observer_];
  }
  interface_ = nil;
}

void WiFiServiceMac::GetProperties(const std::string& network_guid,
                                   base::Value::Dict* properties,
                                   std::string* error) {
  NetworkList::iterator it = FindNetwork(network_guid);
  if (it == networks_.end()) {
    DVLOG(1) << "Network not found:" << network_guid;
    *error = kErrorNotFound;
    return;
  }

  it->connection_state = GetNetworkConnectionState(network_guid);
  *properties = it->ToValue(/*network_list=*/false);
  DVLOG(1) << *properties;
}

void WiFiServiceMac::GetManagedProperties(const std::string& network_guid,
                                          base::Value::Dict* managed_properties,
                                          std::string* error) {
  *error = kErrorNotImplemented;
}

void WiFiServiceMac::GetState(const std::string& network_guid,
                              base::Value::Dict* properties,
                              std::string* error) {
  *error = kErrorNotImplemented;
}

void WiFiServiceMac::SetProperties(const std::string& network_guid,
                                   base::Value::Dict properties,
                                   std::string* error) {
  // If the network properties already exist, don't override previously set
  // properties, unless they are set in |properties|.
  base::Value::Dict* existing_properties =
      network_properties_.FindDict(network_guid);
  if (existing_properties) {
    existing_properties->Merge(std::move(properties));
  } else {
    network_properties_.Set(network_guid, std::move(properties));
  }
}

void WiFiServiceMac::CreateNetwork(bool shared,
                                   base::Value::Dict properties,
                                   std::string* network_guid,
                                   std::string* error) {
  NetworkProperties network_properties;
  if (!network_properties.UpdateFromValue(properties)) {
    *error = kErrorInvalidData;
    return;
  }

  std::string guid = network_properties.ssid;
  if (FindNetwork(guid) != networks_.end()) {
    *error = kErrorInvalidData;
    return;
  }
  network_properties_.Set(guid, std::move(properties));
  *network_guid = guid;
}

void WiFiServiceMac::GetVisibleNetworks(const std::string& network_type,
                                        bool include_details,
                                        base::Value::List* network_list) {
  if (!network_type.empty() &&
      network_type != onc::network_type::kAllTypes &&
      network_type != onc::network_type::kWiFi) {
    return;
  }

  if (networks_.empty())
    UpdateNetworks();

  for (NetworkList::const_iterator it = networks_.begin();
       it != networks_.end();
       ++it) {
    network_list->Append(it->ToValue(/*network_list=*/!include_details));
  }
}

void WiFiServiceMac::RequestNetworkScan() {
  DVLOG(1) << "*** RequestNetworkScan";
  UpdateNetworks();
}

void WiFiServiceMac::StartConnect(const std::string& network_guid,
                                  std::string* error) {
  NSError* ns_error = nil;

  DVLOG(1) << "*** StartConnect: " << network_guid;
  // Remember previously connected network.
  std::string connected_network_guid = GUIDFromSSID([interface_ ssid]);
  // Check whether desired network is already connected.
  if (network_guid == connected_network_guid)
    return;

  NSSet* networks = [interface_
      scanForNetworksWithName:SSIDFromGUID(network_guid)
                        error:&ns_error];

  if (CheckError(ns_error, kErrorScanForNetworksWithName, error))
    return;

  CWNetwork* network = networks.anyObject;
  if (network == nil) {
    // System can't find the network, remove it from the |networks_| and notify
    // observers.
    NetworkList::iterator it = FindNetwork(connected_network_guid);
    if (it != networks_.end()) {
      networks_.erase(it);
      // Notify observers that list has changed.
      NotifyNetworkListChanged(networks_);
    }

    *error = kErrorNotFound;
    return;
  }

  // Check whether WiFi Password is set in |network_properties_|.
  base::Value::Dict* properties = network_properties_.FindDict(network_guid);
  NSString* ns_password = nil;
  if (properties) {
    base::Value::Dict* wifi = properties->FindDict(onc::network_type::kWiFi);
    if (wifi) {
      const std::string* passphrase = wifi->FindString(onc::wifi::kPassphrase);
      if (passphrase)
        ns_password = base::SysUTF8ToNSString(*passphrase);
    }
  }

  // Number of attempts to associate to network.
  static const int kMaxAssociationAttempts = 3;
  // Try to associate to network several times if timeout or PMK error occurs.
  for (int i = 0; i < kMaxAssociationAttempts; ++i) {
    // Nil out the PMK to prevent stale data from causing invalid PMK error
    // (CoreWLANTypes -3924).
    [interface_ setPairwiseMasterKey:nil error:&ns_error];
    if (![interface_ associateToNetwork:network
                              password:ns_password
                                 error:&ns_error]) {
      NSInteger error_code = [ns_error code];
      if (error_code != kCWTimeoutErr && error_code != kCWInvalidPMKErr) {
        break;
      }
    }
  }
  CheckError(ns_error, kErrorAssociateToNetwork, error);
}

void WiFiServiceMac::StartDisconnect(const std::string& network_guid,
                                     std::string* error) {
  DVLOG(1) << "*** StartDisconnect: " << network_guid;

  if (network_guid == GUIDFromSSID([interface_ ssid])) {
    // Power-cycle the interface to disconnect from current network and connect
    // to default network.
    NSError* ns_error = nil;
    [interface_ setPower:NO error:&ns_error];
    CheckError(ns_error, kErrorAssociateToNetwork, error);
    [interface_ setPower:YES error:&ns_error];
    CheckError(ns_error, kErrorAssociateToNetwork, error);
  } else {
    *error = kErrorNotConnected;
  }
}

void WiFiServiceMac::GetKeyFromSystem(const std::string& network_guid,
                                      std::string* key_data,
                                      std::string* error) {
  static const char kAirPortServiceName[] = "AirPort";

  UInt32 password_length = 0;
  void* password_data = nullptr;
  crypto::AppleKeychain keychain;
  OSStatus status = keychain.FindGenericPassword(
      strlen(kAirPortServiceName), kAirPortServiceName, network_guid.length(),
      network_guid.c_str(), &password_length, &password_data, /*item=*/nullptr);
  if (status != errSecSuccess) {
    *error = kErrorNotFound;
    return;
  }

  if (password_data) {
    *key_data = std::string(reinterpret_cast<char*>(password_data),
                            password_length);
    keychain.ItemFreeContent(password_data);
  }
}

void WiFiServiceMac::SetEventObservers(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    NetworkGuidListCallback networks_changed_observer,
    NetworkGuidListCallback network_list_changed_observer) {
  event_task_runner_.swap(task_runner);
  networks_changed_observer_ = std::move(networks_changed_observer);
  network_list_changed_observer_ = std::move(network_list_changed_observer);

  // Remove previous OS notifications observer.
  if (wlan_observer_) {
    [NSNotificationCenter.defaultCenter removeObserver:wlan_observer_];
    wlan_observer_ = nil;
  }

  // Subscribe to OS notifications.
  if (!networks_changed_observer_.is_null()) {
    void (^ns_observer)(NSNotification* notification) = ^(
        NSNotification* notification) {
      DVLOG(1) << "Received CoreWLAN notification that the SSID changed";
      task_runner_->PostTask(
          FROM_HERE, base::BindOnce(&WiFiServiceMac::OnWlanObserverNotification,
                                    base::Unretained(this)));
    };

    // A notification with the symbol kCWSSIDDidChangeNotification started being
    // broadcast on SSID change starting with 10.6 and continuing on through
    // 10.15. However, that symbol was marked as deprecated after macOS 10.10,
    // and actually was removed starting with the macOS 10.9 SDK.
    //
    // Starting with 10.8, a set of parallel notifications with explicitly-
    // specified string names started being broadcast. The parallel notification
    // for that symbol is @"com.apple.coreWLAN.notification.ssid.legacy".
    //
    // Given the choice between a symbol that is marked as "deprecated" in the
    // docs and actually removed from the SDK, and an undocumented string that
    // is secretly broadcast, the string is the safer choice.
    //
    // This is not a supported way to do this. The correct way to do this is the
    // -[CWWiFiClient startMonitoringEventWithType:error:] API:
    // https://developer.apple.com/documentation/corewlan/cwwificlient/1512439-startmonitoringeventwithtype?language=objc
    //
    // TODO(crbug.com/40675519): Switch to using the
    // -[CWWiFiClient startMonitoringEventWithType:error:] API.
    wlan_observer_ = [NSNotificationCenter.defaultCenter
        addObserverForName:@"com.apple.coreWLAN.notification.ssid.legacy"
                    object:nil
                     queue:nil
                usingBlock:ns_observer];
  }
}

void WiFiServiceMac::RequestConnectedNetworkUpdate() {
  OnWlanObserverNotification();
}

void WiFiServiceMac::GetConnectedNetworkSSID(std::string* ssid,
                                             std::string* error) {
  *ssid = base::SysNSStringToUTF8([interface_ ssid]);
  *error = "";
}

std::string WiFiServiceMac::GetNetworkConnectionState(
    const std::string& network_guid) const {
  if (network_guid != GUIDFromSSID([interface_ ssid]))
    return onc::connection_state::kNotConnected;

  // Check whether WiFi network is reachable.
  struct sockaddr_in local_wifi_address;
  bzero(&local_wifi_address, sizeof(local_wifi_address));
  local_wifi_address.sin_len = sizeof(local_wifi_address);
  local_wifi_address.sin_family = AF_INET;
  local_wifi_address.sin_addr.s_addr = htonl(IN_LINKLOCALNETNUM);
  base::apple::ScopedCFTypeRef<SCNetworkReachabilityRef> reachability(
      SCNetworkReachabilityCreateWithAddress(
          kCFAllocatorDefault,
          reinterpret_cast<const struct sockaddr*>(&local_wifi_address)));
  SCNetworkReachabilityFlags flags = 0u;
  if (SCNetworkReachabilityGetFlags(reachability.get(), &flags) &&
      (flags & kSCNetworkReachabilityFlagsReachable) &&
      (flags & kSCNetworkReachabilityFlagsIsDirect)) {
    // Network is reachable, report is as |kConnected|.
    return onc::connection_state::kConnected;
  }
  // Network is not reachable yet, so it must be |kConnecting|.
  return onc::connection_state::kConnecting;
}

void WiFiServiceMac::UpdateNetworks() {
  NSError* ns_error = nil;
  NSSet* cw_networks = [interface_ scanForNetworksWithName:nil
                                                     error:&ns_error];
  if (ns_error != nil)
    return;

  std::string connected_bssid = base::SysNSStringToUTF8([interface_ bssid]);
  std::map<std::string, NetworkProperties*> network_properties_map;
  networks_.clear();

  // There is one |cw_network| per BSS in |cw_networks|, so go through the set
  // and combine them, paying attention to supported frequencies.
  for (CWNetwork* cw_network in cw_networks) {
    std::string network_guid = GUIDFromSSID([cw_network ssid]);
    bool update_all_properties = false;

    if (network_properties_map.find(network_guid) ==
            network_properties_map.end()) {
      networks_.emplace_back();
      network_properties_map[network_guid] = &networks_.back();
      update_all_properties = true;
    }
    // If current network is connected, use its properties for this network.
    if (base::SysNSStringToUTF8(cw_network.bssid) == connected_bssid) {
      update_all_properties = true;
    }

    NetworkProperties* properties = network_properties_map.at(network_guid);
    if (update_all_properties) {
      NetworkPropertiesFromCWNetwork(cw_network, properties);
    } else {
      properties->frequency_set.insert(
          FrequencyFromCWChannelBand(cw_network.wlanChannel.channelBand));
    }
  }
  // Sort networks, so connected/connecting is up front.
  networks_.sort(NetworkProperties::OrderByType);
  // Notify observers that list has changed.
  NotifyNetworkListChanged(networks_);
}

bool WiFiServiceMac::CheckError(NSError* ns_error,
                                const char* error_name,
                                std::string* error) const {
  if (ns_error != nil) {
    DLOG(ERROR) << "*** Error:" << error_name << ":" << [ns_error code];
    *error = error_name;
    return true;
  }
  return false;
}

void WiFiServiceMac::NetworkPropertiesFromCWNetwork(
    const CWNetwork* network,
    NetworkProperties* properties) const {
  std::string network_guid = GUIDFromSSID(network.ssid);

  properties->connection_state = GetNetworkConnectionState(network_guid);
  properties->ssid = base::SysNSStringToUTF8(network.ssid);
  properties->name = properties->ssid;
  properties->guid = network_guid;
  properties->type = onc::network_type::kWiFi;

  properties->bssid = base::SysNSStringToUTF8(network.bssid);
  properties->frequency = FrequencyFromCWChannelBand(
      static_cast<CWChannelBand>(network.wlanChannel.channelBand));
  properties->frequency_set.insert(properties->frequency);

  properties->security = SecurityFromCWNetwork(network);
  properties->signal_strength = network.rssiValue;
}

std::string WiFiServiceMac::SecurityFromCWNetwork(
    const CWNetwork* network) const {
  if ([network supportsSecurity:kCWSecurityWPAEnterprise] ||
      [network supportsSecurity:kCWSecurityWPA2Enterprise]) {
    return onc::wifi::kWPA_EAP;
  }

  if ([network supportsSecurity:kCWSecurityWPAPersonal] ||
      [network supportsSecurity:kCWSecurityWPA2Personal]) {
    return onc::wifi::kWPA_PSK;
  }

  if ([network supportsSecurity:kCWSecurityWEP])
    return onc::wifi::kWEP_PSK;

  if ([network supportsSecurity:kCWSecurityNone])
    return onc::wifi::kSecurityNone;

  // TODO(mef): Figure out correct mapping.
  if ([network supportsSecurity:kCWSecurityDynamicWEP])
    return onc::wifi::kWPA_EAP;

  return onc::wifi::kWPA_EAP;
}

Frequency WiFiServiceMac::FrequencyFromCWChannelBand(CWChannelBand band) const {
  return band == kCWChannelBand2GHz ? kFrequency2400 : kFrequency5000;
}

NetworkList::iterator WiFiServiceMac::FindNetwork(
    const std::string& network_guid) {
  for (NetworkList::iterator it = networks_.begin();
       it != networks_.end();
       ++it) {
    if (it->guid == network_guid)
      return it;
  }
  return networks_.end();
}

void WiFiServiceMac::OnWlanObserverNotification() {
  std::string connected_network_guid = GUIDFromSSID([interface_ ssid]);
  DVLOG(1) << " *** Got Notification: " << connected_network_guid;
  // Connected network has changed, mark previous one disconnected.
  if (connected_network_guid != connected_network_guid_) {
    // Update connection_state of newly connected network.
    NetworkList::iterator it = FindNetwork(connected_network_guid_);
    if (it != networks_.end()) {
      it->connection_state = onc::connection_state::kNotConnected;
      NotifyNetworkChanged(connected_network_guid_);
    }
    connected_network_guid_ = connected_network_guid;
  }

  if (!connected_network_guid.empty()) {
    // Update connection_state of newly connected network.
    NetworkList::iterator it = FindNetwork(connected_network_guid);
    if (it != networks_.end()) {
      it->connection_state = GetNetworkConnectionState(connected_network_guid);
    } else {
      // Can't find |connected_network_guid| in |networks_|, try to update it.
      UpdateNetworks();
    }
    // Notify that network is connecting.
    NotifyNetworkChanged(connected_network_guid);
    // Further network change notification will be sent by detector.
  }
}

void WiFiServiceMac::NotifyNetworkListChanged(const NetworkList& networks) {
  if (!network_list_changed_observer_)
    return;

  NetworkGuidList current_networks;
  for (const auto& network : networks) {
    current_networks.push_back(network.guid);
  }

  event_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(network_list_changed_observer_, current_networks));
}

void WiFiServiceMac::NotifyNetworkChanged(const std::string& network_guid) {
  if (!networks_changed_observer_)
    return;

  DVLOG(1) << "NotifyNetworkChanged: " << network_guid;
  NetworkGuidList changed_networks(1, network_guid);
  event_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(networks_changed_observer_, changed_networks));
}

// static
WiFiService* WiFiService::Create() { return new WiFiServiceMac(); }

}  // namespace wifi
