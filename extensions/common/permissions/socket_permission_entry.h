// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef EXTENSIONS_COMMON_PERMISSIONS_SOCKET_PERMISSION_ENTRY_H_
#define EXTENSIONS_COMMON_PERMISSIONS_SOCKET_PERMISSION_ENTRY_H_

#include <string>
#include <vector>

#include "content/public/common/socket_permission_request.h"
#include "ipc/ipc_param_traits.h"

namespace ipc_fuzzer {
template <class T>
struct FuzzTraits;
template <class T>
struct GenerateTraits;
}  // namespace ipc_fuzzer

namespace extensions {

// Internal representation of a socket permission for a specific operation, such
// as UDP "bind", host 127.0.0.1, port *.
class SocketPermissionEntry {
 public:
  enum HostType { ANY_HOST, HOSTS_IN_DOMAINS, SPECIFIC_HOSTS, };

  SocketPermissionEntry();
  ~SocketPermissionEntry();

  // operators <, == are needed by container std::set and algorithms
  // std::set_includes and std::set_differences.
  bool operator<(const SocketPermissionEntry& rhs) const;
  bool operator==(const SocketPermissionEntry& rhs) const;

  bool Check(const content::SocketPermissionRequest& request) const;

  // Parse a host:port pattern for a given operation type.
  //   <pattern> := '' |
  //                <host> |
  //                ':' <port> |
  //                <host> ':' <port> |
  //
  //   <host> := '*' |
  //             '*.' <anychar except '/' and '*'>+ |
  //             <anychar except '/' and '*'>+
  //
  //   <port> := '*' |
  //             <port number between 0 and 65535>)
  static bool ParseHostPattern(
      content::SocketPermissionRequest::OperationType type,
      const std::string& pattern,
      SocketPermissionEntry* entry);

  static bool ParseHostPattern(
      content::SocketPermissionRequest::OperationType type,
      const std::vector<std::string>& pattern_tokens,
      SocketPermissionEntry* entry);

  // Returns true if the permission type can be bound to a host or port.
  bool IsAddressBoundType() const;

  std::string GetHostPatternAsString() const;
  HostType GetHostType() const;

  const content::SocketPermissionRequest& pattern() const { return pattern_; }
  bool match_subdomains() const { return match_subdomains_; }

 private:
  // Friend so ParamTraits can serialize us.
  friend struct IPC::ParamTraits<SocketPermissionEntry>;
  friend struct ipc_fuzzer::FuzzTraits<SocketPermissionEntry>;
  friend struct ipc_fuzzer::GenerateTraits<SocketPermissionEntry>;

  // The permission type, host and port.
  content::SocketPermissionRequest pattern_;

  // True if there was a wildcard in the host name.
  bool match_subdomains_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_PERMISSIONS_SOCKET_PERMISSION_ENTRY_H_
