// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_NETWORK_LIST_H_
#define PPAPI_CPP_NETWORK_LIST_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "ppapi/c/ppb_network_list.h"
#include "ppapi/cpp/pass_ref.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class NetAddress;

class NetworkList : public Resource {
 public:
  NetworkList();
  NetworkList(PassRef, PP_Resource resource);

  /// Returns true if the required interface is available.
  static bool IsAvailable();

  /// @return Returns the number of available network interfaces or 0
  /// if the list has never been updated.
  uint32_t GetCount() const;

  /// @return Returns the name for the network interface with the
  /// specified <code>index</code>.
  std::string GetName(uint32_t index) const;

  /// @return Returns the type of the network interface with the
  /// specified <code>index</code>.
  PP_NetworkList_Type GetType(uint32_t index) const;

  /// @return Returns the current state of the network interface with
  /// the specified <code>index</code>.
  PP_NetworkList_State GetState(uint32_t index) const;

  /// Gets the list of IP addresses for the network interface with the
  /// specified <code>index</code> and stores them in
  /// <code>addresses</code>.
  int32_t GetIpAddresses(uint32_t index,
                         std::vector<NetAddress>* addresses) const;

  /// @return Returns the display name for the network interface with
  /// the specified <code>index</code>.
  std::string GetDisplayName(uint32_t index) const;

  /// @return Returns the MTU for the network interface with the
  /// specified <code>index</code>.
  uint32_t GetMTU(uint32_t index) const;
};

}  // namespace pp

#endif  // PPAPI_CPP_NETWORK_LIST_H_
