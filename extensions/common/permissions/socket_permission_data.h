// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef EXTENSIONS_COMMON_PERMISSIONS_SOCKET_PERMISSION_DATA_H_
#define EXTENSIONS_COMMON_PERMISSIONS_SOCKET_PERMISSION_DATA_H_

#include <string>

#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/socket_permission_entry.h"
#include "ipc/ipc_param_traits.h"

namespace ipc_fuzzer {
template <class T>
struct FuzzTraits;
template <class T>
struct GenerateTraits;
}  // namespace ipc_fuzzer

namespace extensions {

// A pattern that can be used to match socket permission.
//   <socket-permission-pattern>
//          := <op> |
//             <op> ':' <host> |
//             <op> ':' ':' <port> |
//             <op> ':' <host> ':' <port> |
//             'udp-multicast-membership'
//   <op>   := 'tcp-connect' |
//             'tcp-listen' |
//             'udp-bind' |
//             'udp-send-to' |
//             'udp-multicast-membership' |
//             'resolve-host' |
//             'resolve-proxy' |
//             'network-state'
//   <host> := '*' |
//             '*.' <anychar except '/' and '*'>+ |
//             <anychar except '/' and '*'>+
//   <port> := '*' |
//             <port number between 0 and 65535>)
// The multicast membership permission implies a permission to any address.
class SocketPermissionData {
 public:
  SocketPermissionData();
  ~SocketPermissionData();

  // operators <, == are needed by container std::set and algorithms
  // std::set_includes and std::set_differences.
  bool operator<(const SocketPermissionData& rhs) const;
  bool operator==(const SocketPermissionData& rhs) const;

  // Check if |param| (which must be a SocketPermissionData::CheckParam)
  // matches the spec of |this|.
  bool Check(const APIPermission::CheckParam* param) const;

  // Convert |this| into a base::Value.
  std::unique_ptr<base::Value> ToValue() const;

  // Populate |this| from a base::Value.
  bool FromValue(const base::Value* value);

  // TODO(bryeung): SocketPermissionData should be encoded as a base::Value
  // instead of a string.  Until that is done, expose these methods for
  // testing.
  bool ParseForTest(const std::string& permission) { return Parse(permission); }
  const std::string& GetAsStringForTest() const { return GetAsString(); }

  const SocketPermissionEntry& entry() const { return entry_; }

 private:
  // Friend so ParamTraits can serialize us.
  friend struct IPC::ParamTraits<SocketPermissionData>;
  friend struct ipc_fuzzer::FuzzTraits<SocketPermissionData>;
  friend struct ipc_fuzzer::GenerateTraits<SocketPermissionData>;

  SocketPermissionEntry& entry();

  bool Parse(const std::string& permission);
  const std::string& GetAsString() const;
  void Reset();

  SocketPermissionEntry entry_;
  mutable std::string spec_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_PERMISSIONS_SOCKET_PERMISSION_DATA_H_
