// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/host_info.h"

#include "base/logging.h"

namespace remoting {
namespace test {

HostInfo::HostInfo() = default;
HostInfo::HostInfo(const HostInfo& other) = default;

HostInfo::~HostInfo() = default;

bool HostInfo::ParseHostInfo(const base::Value& host_info) {
  // Add TokenUrlPatterns to HostInfo.
  const base::Value* list_value = host_info.FindListKey("tokenUrlPatterns");
  if (list_value) {
    for (const base::Value& item : list_value->GetList()) {
      if (!item.is_string()) {
        return false;
      }
      token_url_patterns.push_back(item.GetString());
    }
  }

  const std::string* string_value;

  string_value = host_info.FindStringKey("status");
  if (string_value && *string_value == "ONLINE") {
    status = kHostStatusOnline;
  } else if (string_value && *string_value == "OFFLINE") {
    status = kHostStatusOffline;
  } else {
    LOG(ERROR) << "Response Status is "
               << (string_value ? *string_value : "<unset>");
    return false;
  }

  string_value = host_info.FindStringKey("hostId");
  if (string_value) {
    host_id = *string_value;
  } else {
    LOG(ERROR) << "hostId was not found in host_info";
    return false;
  }

  string_value = host_info.FindStringKey("hostName");
  if (string_value) {
    host_name = *string_value;
  } else {
    LOG(ERROR) << "hostName was not found in host_info";
    return false;
  }

  string_value = host_info.FindStringKey("publicKey");
  if (string_value) {
    public_key = *string_value;
  } else {
    LOG(ERROR) << "publicKey was not found for " << host_name;
    return false;
  }

  // If the host entry was created but the host was never online, then the jid
  // is never set.
  string_value = host_info.FindStringKey("jabberId");
  if (string_value) {
    host_jid = *string_value;
  } else if (status == kHostStatusOnline) {
    LOG(ERROR) << host_name << " is online but is missing a jabberId";
    return false;
  }

  string_value = host_info.FindStringKey("hostOfflineReason");
  if (string_value)
    offline_reason = *string_value;

  return true;
}

bool HostInfo::IsReadyForConnection() const {
  return !host_jid.empty() && status == kHostStatusOnline;
}

ConnectionSetupInfo HostInfo::GenerateConnectionSetupInfo(
    const std::string& access_token,
    const std::string& user_name,
    const std::string& pin) const {
  ConnectionSetupInfo connection_setup_info;
  connection_setup_info.access_token = access_token;
  connection_setup_info.host_id = host_id;
  connection_setup_info.host_jid = host_jid;
  connection_setup_info.host_name = host_name;
  connection_setup_info.pin = pin;
  connection_setup_info.public_key = public_key;
  connection_setup_info.user_name = user_name;
  return connection_setup_info;
}

}  // namespace test
}  // namespace remoting
