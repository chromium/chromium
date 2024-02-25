// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/sockets/sockets_manifest_permission.h"

#include <memory>

#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/api/extensions_manifest_types.h"
#include "extensions/common/api/sockets/sockets_manifest_data.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "ipc/ipc_message.h"
#include "ui/base/l10n/l10n_util.h"

using extensions::mojom::APIPermissionID;

namespace extensions {

namespace sockets_errors {
const char kErrorInvalidHostPattern[] = "Invalid host:port pattern '*'";
}

using api::extensions_manifest_types::Sockets;
using api::extensions_manifest_types::SocketHostPatterns;
using content::SocketPermissionRequest;

namespace {

static bool ParseHostPattern(
    SocketsManifestPermission* permission,
    content::SocketPermissionRequest::OperationType operation_type,
    const std::string& host_pattern,
    std::u16string* error) {
  SocketPermissionEntry entry;
  if (!SocketPermissionEntry::ParseHostPattern(
          operation_type, host_pattern, &entry)) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        sockets_errors::kErrorInvalidHostPattern, host_pattern);
    return false;
  }
  permission->AddPermission(entry);
  return true;
}

static bool ParseHostPatterns(
    SocketsManifestPermission* permission,
    content::SocketPermissionRequest::OperationType operation_type,
    const std::optional<SocketHostPatterns>& host_patterns,
    std::u16string* error) {
  if (!host_patterns)
    return true;

  if (host_patterns->as_string) {
    return ParseHostPattern(
        permission, operation_type, *host_patterns->as_string, error);
  }

  CHECK(host_patterns->as_strings);
  for (std::vector<std::string>::const_iterator it =
           host_patterns->as_strings->begin();
       it != host_patterns->as_strings->end();
       ++it) {
    if (!ParseHostPattern(permission, operation_type, *it, error)) {
      return false;
    }
  }
  return true;
}

static void SetHostPatterns(
    std::optional<SocketHostPatterns>& host_patterns,
    const SocketsManifestPermission* permission,
    content::SocketPermissionRequest::OperationType operation_type) {
  host_patterns.emplace();
  host_patterns->as_strings.emplace();
  for (auto it = permission->entries().cbegin();
       it != permission->entries().cend(); ++it) {
    if (it->pattern().type == operation_type) {
      host_patterns->as_strings->push_back(it->GetHostPatternAsString());
    }
  }
}

// Helper function for adding the 'any host' permission. Determines if the
// message is needed from |sockets|, and adds the permission to |ids|.
// Returns true if it added the message.
bool AddAnyHostMessage(const SocketPermissionEntrySet& sockets,
                       PermissionIDSet* ids) {
  for (const auto& socket : sockets) {
    if (socket.IsAddressBoundType() &&
        socket.GetHostType() == SocketPermissionEntry::ANY_HOST) {
      ids->insert(APIPermissionID::kSocketAnyHost);
      return true;
    }
  }
  return false;
}

// Helper function for adding subdomain socket permissions. Determines what
// messages are needed from |sockets|, and adds permissions to |ids|.
void AddSubdomainHostMessage(const SocketPermissionEntrySet& sockets,
                             PermissionIDSet* ids) {
  std::set<std::u16string> domains;
  for (const auto& socket : sockets) {
    if (socket.GetHostType() == SocketPermissionEntry::HOSTS_IN_DOMAINS)
      domains.insert(base::UTF8ToUTF16(socket.pattern().host));
  }
  if (!domains.empty()) {
    for (const auto& domain : domains)
      ids->insert(APIPermissionID::kSocketDomainHosts, domain);
  }
}

// Helper function for adding specific host socket permissions. Determines what
// messages are needed from |sockets|, and adds permissions to |ids|.
void AddSpecificHostMessage(const SocketPermissionEntrySet& sockets,
                            PermissionIDSet* ids) {
  std::set<std::u16string> hostnames;
  for (const auto& socket : sockets) {
    if (socket.GetHostType() == SocketPermissionEntry::SPECIFIC_HOSTS)
      hostnames.insert(base::UTF8ToUTF16(socket.pattern().host));
  }
  if (!hostnames.empty()) {
    for (const auto& hostname : hostnames)
      ids->insert(APIPermissionID::kSocketSpecificHosts, hostname);
  }
}

// Helper function for adding the network list socket permission. Determines if
// the message is needed from |sockets|, and adds the permission to |ids|.
void AddNetworkListMessage(const SocketPermissionEntrySet& sockets,
                           PermissionIDSet* ids) {
  for (const auto& socket : sockets) {
    if (socket.pattern().type == SocketPermissionRequest::NETWORK_STATE) {
      ids->insert(APIPermissionID::kNetworkState);
    }
  }
}

}  // namespace

SocketsManifestPermission::SocketsManifestPermission() = default;

SocketsManifestPermission::~SocketsManifestPermission() = default;

// static
std::unique_ptr<SocketsManifestPermission> SocketsManifestPermission::FromValue(
    const base::Value& value,
    std::u16string* error) {
  auto sockets = Sockets::FromValue(value);
  if (!sockets.has_value()) {
    *error = std::move(sockets).error();
    return nullptr;
  }

  std::unique_ptr<SocketsManifestPermission> result(
      new SocketsManifestPermission());
  if (sockets->udp) {
    if (!ParseHostPatterns(result.get(),
                           SocketPermissionRequest::UDP_BIND,
                           sockets->udp->bind,
                           error)) {
      return nullptr;
    }
    if (!ParseHostPatterns(result.get(),
                           SocketPermissionRequest::UDP_SEND_TO,
                           sockets->udp->send,
                           error)) {
      return nullptr;
    }
    if (!ParseHostPatterns(result.get(),
                           SocketPermissionRequest::UDP_MULTICAST_MEMBERSHIP,
                           sockets->udp->multicast_membership,
                           error)) {
      return nullptr;
    }
  }
  if (sockets->tcp) {
    if (!ParseHostPatterns(result.get(),
                           SocketPermissionRequest::TCP_CONNECT,
                           sockets->tcp->connect,
                           error)) {
      return nullptr;
    }
  }
  if (sockets->tcp_server) {
    if (!ParseHostPatterns(result.get(),
                           SocketPermissionRequest::TCP_LISTEN,
                           sockets->tcp_server->listen,
                           error)) {
      return nullptr;
    }
  }
  return result;
}

bool SocketsManifestPermission::CheckRequest(
    const Extension* extension,
    const SocketPermissionRequest& request) const {
  for (auto it = permissions_.cbegin(); it != permissions_.cend(); ++it) {
    if (it->Check(request))
      return true;
  }
  return false;
}

std::string SocketsManifestPermission::name() const {
  return manifest_keys::kSockets;
}

std::string SocketsManifestPermission::id() const { return name(); }

PermissionIDSet SocketsManifestPermission::GetPermissions() const {
  PermissionIDSet ids;
  AddSocketHostPermissions(permissions_, &ids);
  return ids;
}

bool SocketsManifestPermission::FromValue(const base::Value* value) {
  if (!value)
    return false;
  std::u16string error;
  std::unique_ptr<SocketsManifestPermission> manifest_permission(
      SocketsManifestPermission::FromValue(*value, &error));

  if (!manifest_permission)
    return false;

  permissions_ = manifest_permission->permissions_;
  return true;
}

std::unique_ptr<base::Value> SocketsManifestPermission::ToValue() const {
  Sockets sockets;

  sockets.udp.emplace();
  SetHostPatterns(sockets.udp->bind, this, SocketPermissionRequest::UDP_BIND);
  SetHostPatterns(
      sockets.udp->send, this, SocketPermissionRequest::UDP_SEND_TO);
  SetHostPatterns(sockets.udp->multicast_membership,
                  this,
                  SocketPermissionRequest::UDP_MULTICAST_MEMBERSHIP);
  if (sockets.udp->bind->as_strings->size() == 0 &&
      sockets.udp->send->as_strings->size() == 0 &&
      sockets.udp->multicast_membership->as_strings->size() == 0) {
    sockets.udp.reset();
  }

  sockets.tcp.emplace();
  SetHostPatterns(
      sockets.tcp->connect, this, SocketPermissionRequest::TCP_CONNECT);
  if (sockets.tcp->connect->as_strings->size() == 0) {
    sockets.tcp.reset();
  }

  sockets.tcp_server.emplace();
  SetHostPatterns(
      sockets.tcp_server->listen, this, SocketPermissionRequest::TCP_LISTEN);
  if (sockets.tcp_server->listen->as_strings->size() == 0) {
    sockets.tcp_server.reset();
  }

  return base::Value::ToUniquePtrValue(base::Value(sockets.ToValue()));
}

std::unique_ptr<ManifestPermission> SocketsManifestPermission::Diff(
    const ManifestPermission* rhs) const {
  const SocketsManifestPermission* other =
      static_cast<const SocketsManifestPermission*>(rhs);

  auto result = std::make_unique<SocketsManifestPermission>();
  result->permissions_ = base::STLSetDifference<SocketPermissionEntrySet>(
      permissions_, other->permissions_);
  return result;
}

std::unique_ptr<ManifestPermission> SocketsManifestPermission::Union(
    const ManifestPermission* rhs) const {
  const SocketsManifestPermission* other =
      static_cast<const SocketsManifestPermission*>(rhs);

  auto result = std::make_unique<SocketsManifestPermission>();
  result->permissions_ = base::STLSetUnion<SocketPermissionEntrySet>(
      permissions_, other->permissions_);
  return result;
}

std::unique_ptr<ManifestPermission> SocketsManifestPermission::Intersect(
    const ManifestPermission* rhs) const {
  const SocketsManifestPermission* other =
      static_cast<const SocketsManifestPermission*>(rhs);

  auto result = std::make_unique<SocketsManifestPermission>();
  result->permissions_ = base::STLSetIntersection<SocketPermissionEntrySet>(
      permissions_, other->permissions_);
  return result;
}

void SocketsManifestPermission::AddPermission(
    const SocketPermissionEntry& entry) {
  permissions_.insert(entry);
}

// static
void SocketsManifestPermission::AddSocketHostPermissions(
    const SocketPermissionEntrySet& sockets,
    PermissionIDSet* ids) {
  if (!AddAnyHostMessage(sockets, ids)) {
    AddSpecificHostMessage(sockets, ids);
    AddSubdomainHostMessage(sockets, ids);
  }
  AddNetworkListMessage(sockets, ids);
}

bool SocketsManifestPermission::RequiresManagementUIWarning() const {
  return false;
}

bool SocketsManifestPermission::RequiresManagedSessionFullLoginWarning() const {
  return false;
}

}  // namespace extensions
