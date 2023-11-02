// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string.h>
#include <sstream>

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/net_address.h"
#include "ppapi/cpp/network_list.h"
#include "ppapi/cpp/network_monitor.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/var_array.h"
#include "ppapi/cpp/var_dictionary.h"
#include "ppapi/utility/completion_callback_factory.h"

#ifdef WIN32
#undef PostMessage
// Allow 'this' in initializer list
#pragma warning(disable : 4355)
#endif

class NetworkMonitorInstance : public pp::Instance {
 public:
  explicit NetworkMonitorInstance(PP_Instance instance)
      : pp::Instance(instance),
        callback_factory_(this),
        network_monitor_(this) {}

  virtual ~NetworkMonitorInstance() {}
  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]);

 private:
  virtual void OnUpdateNetworkList(int32_t result,
                                   pp::NetworkList network_list);
  static std::string GetNetworkStateAsString(PP_NetworkList_State state);
  static std::string GetNetworkTypeAsString(PP_NetworkList_Type type);
  static std::string GetNetAddressAsString(pp::NetAddress address);

  pp::CompletionCallbackFactory<NetworkMonitorInstance> callback_factory_;
  pp::NetworkMonitor network_monitor_;
};

bool NetworkMonitorInstance::Init(uint32_t argc,
                                  const char* argn[],
                                  const char* argv[]) {
  // Start listing for network updates.
  int32_t result = network_monitor_.UpdateNetworkList(
      callback_factory_.NewCallbackWithOutput(
          &NetworkMonitorInstance::OnUpdateNetworkList));
  if (result != PP_OK_COMPLETIONPENDING) {
    std::ostringstream status;
    status << "UpdateNetworkList failed: " << result;
    PostMessage(status.str());
  }

  return true;
}

void NetworkMonitorInstance::OnUpdateNetworkList(int32_t result,
                                                 pp::NetworkList network_list) {
  // Send the new network list to JavaScript.
  if (result < 0) {
    std::ostringstream status;
    status << "UpdateNetworkList failed: " << result;
    PostMessage(status.str());
    return;
  }

  pp::VarArray var_network_list;
  uint32_t count = network_list.GetCount();
  for (uint32_t i = 0; i < count; ++i) {
    pp::VarDictionary var_network;
    var_network.Set("displayName", network_list.GetDisplayName(i));
    var_network.Set("name", network_list.GetName(i));
    var_network.Set("state", GetNetworkStateAsString(network_list.GetState(i)));
    var_network.Set("type", GetNetworkTypeAsString(network_list.GetType(i)));
    var_network.Set("MTU", static_cast<int32_t>(network_list.GetMTU(i)));

    pp::VarArray var_ip_addresses;
    std::vector<pp::NetAddress> ip_addresses;
    result = network_list.GetIpAddresses(i, &ip_addresses);
    if (result == PP_OK) {
      for (size_t i = 0; i < ip_addresses.size(); ++i) {
        var_ip_addresses.Set(i, GetNetAddressAsString(ip_addresses[i]));
      }
    } else {
      // Call to GetIpAddresses failed, just give an empty list.
    }

    var_network.Set("ipAddresses", var_ip_addresses);
    var_network_list.Set(i, var_network);
  }

  PostMessage(var_network_list);
}

// static
std::string NetworkMonitorInstance::GetNetworkStateAsString(
    PP_NetworkList_State state) {
  switch (state) {
    case PP_NETWORKLIST_STATE_UP:
      return "up";

    case PP_NETWORKLIST_STATE_DOWN:
      return "down";

    default:
      return "invalid";
  }
}

// static
std::string NetworkMonitorInstance::GetNetworkTypeAsString(
    PP_NetworkList_Type type) {
  switch (type) {
    case PP_NETWORKLIST_TYPE_ETHERNET:
      return "ethernet";

    case PP_NETWORKLIST_TYPE_WIFI:
      return "wifi";

    case PP_NETWORKLIST_TYPE_CELLULAR:
      return "cellular";

    case PP_NETWORKLIST_TYPE_UNKNOWN:
      return "unknown";

    default:
      return "invalid";
  }
}

// static
std::string NetworkMonitorInstance::GetNetAddressAsString(
    pp::NetAddress address) {
  bool include_port = true;
  return address.DescribeAsString(include_port).AsString();
}

class NetworkMonitorModule : public pp::Module {
 public:
  NetworkMonitorModule() : pp::Module() {}
  virtual ~NetworkMonitorModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new NetworkMonitorInstance(instance);
  }
};

namespace pp {
Module* CreateModule() { return new NetworkMonitorModule(); }
}  // namespace pp
