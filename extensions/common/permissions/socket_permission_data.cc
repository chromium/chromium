// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/permissions/socket_permission_data.h"

#include <cstdlib>
#include <memory>
#include <sstream>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/socket_permission.h"
#include "url/url_canon.h"

namespace {

using content::SocketPermissionRequest;
using extensions::SocketPermissionData;

const char kInvalid[] = "invalid";
const char kTCPConnect[] = "tcp-connect";
const char kTCPListen[] = "tcp-listen";
const char kUDPBind[] = "udp-bind";
const char kUDPSendTo[] = "udp-send-to";
const char kUDPMulticastMembership[] = "udp-multicast-membership";
const char kResolveHost[] = "resolve-host";
const char kResolveProxy[] = "resolve-proxy";
const char kNetworkState[] = "network-state";

SocketPermissionRequest::OperationType StringToType(const std::string& s) {
  if (s == kTCPConnect)
    return SocketPermissionRequest::TCP_CONNECT;
  if (s == kTCPListen)
    return SocketPermissionRequest::TCP_LISTEN;
  if (s == kUDPBind)
    return SocketPermissionRequest::UDP_BIND;
  if (s == kUDPSendTo)
    return SocketPermissionRequest::UDP_SEND_TO;
  if (s == kUDPMulticastMembership)
    return SocketPermissionRequest::UDP_MULTICAST_MEMBERSHIP;
  if (s == kResolveHost)
    return SocketPermissionRequest::RESOLVE_HOST;
  if (s == kResolveProxy)
    return SocketPermissionRequest::RESOLVE_PROXY;
  if (s == kNetworkState)
    return SocketPermissionRequest::NETWORK_STATE;
  return SocketPermissionRequest::NONE;
}

const char* TypeToString(SocketPermissionRequest::OperationType type) {
  switch (type) {
    case SocketPermissionRequest::TCP_CONNECT:
      return kTCPConnect;
    case SocketPermissionRequest::TCP_LISTEN:
      return kTCPListen;
    case SocketPermissionRequest::UDP_BIND:
      return kUDPBind;
    case SocketPermissionRequest::UDP_SEND_TO:
      return kUDPSendTo;
    case SocketPermissionRequest::UDP_MULTICAST_MEMBERSHIP:
      return kUDPMulticastMembership;
    case SocketPermissionRequest::RESOLVE_HOST:
      return kResolveHost;
    case SocketPermissionRequest::RESOLVE_PROXY:
      return kResolveProxy;
    case SocketPermissionRequest::NETWORK_STATE:
      return kNetworkState;
    default:
      return kInvalid;
  }
}

}  // namespace

namespace extensions {

SocketPermissionData::SocketPermissionData() = default;

SocketPermissionData::~SocketPermissionData() = default;

bool SocketPermissionData::operator<(const SocketPermissionData& rhs) const {
  return entry_ < rhs.entry_;
}

bool SocketPermissionData::operator==(const SocketPermissionData& rhs) const {
  return entry_ == rhs.entry_;
}

bool SocketPermissionData::Check(const APIPermission::CheckParam* param) const {
  if (!param)
    return false;
  const SocketPermission::CheckParam& specific_param =
      *static_cast<const SocketPermission::CheckParam*>(param);
  const SocketPermissionRequest& request = specific_param.request;

  return entry_.Check(request);
}

std::unique_ptr<base::Value> SocketPermissionData::ToValue() const {
  return std::make_unique<base::Value>(GetAsString());
}

bool SocketPermissionData::FromValue(const base::Value* value) {
  if (!value->is_string())
    return false;

  return Parse(value->GetString());
}

SocketPermissionEntry& SocketPermissionData::entry() {
  // Clear the spec because the caller could mutate |this|.
  spec_.clear();
  return entry_;
}

// TODO(reillyg): Rewrite this method to support IPv6.
bool SocketPermissionData::Parse(const std::string& permission) {
  Reset();

  std::vector<std::string> tokens = base::SplitString(
      permission, ":", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (tokens.empty())
    return false;

  SocketPermissionRequest::OperationType type = StringToType(tokens[0]);
  if (type == SocketPermissionRequest::NONE)
    return false;

  tokens.erase(tokens.begin());
  return SocketPermissionEntry::ParseHostPattern(type, tokens, &entry_);
}

const std::string& SocketPermissionData::GetAsString() const {
  if (!spec_.empty())
    return spec_;

  spec_.reserve(64);
  spec_.append(TypeToString(entry_.pattern().type));
  std::string pattern = entry_.GetHostPatternAsString();
  if (!pattern.empty()) {
    spec_.append(1, ':').append(pattern);
  }
  return spec_;
}

void SocketPermissionData::Reset() {
  entry_ = SocketPermissionEntry();
  spec_.clear();
}

}  // namespace extensions
