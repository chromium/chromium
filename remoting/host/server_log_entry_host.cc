// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/server_log_entry_host.h"

#include "base/strings/stringize_macros.h"
#include "remoting/host/host_details.h"
#include "remoting/signaling/server_log_entry.h"

namespace remoting {

namespace {
const char kValueEventNameSessionState[] = "session-state";

const char kValueRoleHost[] = "host";

const char kKeySessionState[] = "session-state";
const char kValueSessionStateConnected[] = "connected";
const char kValueSessionStateClosed[] = "closed";

const char kKeyOsName[] = "os-name";
const char kKeyOsVersion[] = "os-version";

const char kKeyHostVersion[] = "host-version";

const char kKeyConnectionType[] = "connection-type";

const char* GetValueSessionState(bool connected) {
  return connected ? kValueSessionStateConnected : kValueSessionStateClosed;
}

}  // namespace

std::unique_ptr<ServerLogEntry> MakeLogEntryForSessionStateChange(
    bool connected) {
  std::unique_ptr<ServerLogEntry> entry(new ServerLogEntry());
  entry->AddRoleField(kValueRoleHost);
  entry->AddEventNameField(kValueEventNameSessionState);
  entry->Set(kKeySessionState, GetValueSessionState(connected));
  return entry;
}

void AddHostFieldsToLogEntry(ServerLogEntry* entry) {
  // TODO os name, os version, and version will be in the main message body,
  // remove these fields at a later date to remove redundancy.
  entry->Set(kKeyOsName, GetHostOperatingSystemName());
  entry->Set(kKeyOsVersion, GetHostOperatingSystemVersion());
  entry->Set(kKeyHostVersion, STRINGIZE(VERSION));
  entry->AddCpuField();
}

void AddConnectionTypeToLogEntry(ServerLogEntry* entry,
                                 protocol::TransportRoute::RouteType type) {
  entry->Set(kKeyConnectionType, protocol::TransportRoute::GetTypeString(type));
}

}  // namespace remoting
