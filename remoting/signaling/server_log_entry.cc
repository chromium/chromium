// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/server_log_entry.h"

#include "base/notreached.h"
#include "base/system/sys_info.h"
#include "remoting/base/constants.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

using base::SysInfo;
using jingle_xmpp::QName;
using jingle_xmpp::XmlElement;

namespace remoting {

namespace {

const char kLogCommand[] = "log";
const char kLogEntry[] = "entry";

const char kKeyEventName[] = "event-name";

const char kKeyRole[] = "role";

const char kKeyMode[] = "mode";
const char kValueModeIt2Me[] = "it2me";
const char kValueModeMe2Me[] = "me2me";

const char kKeyCpu[] = "cpu";

}  // namespace

ServerLogEntry::ServerLogEntry() = default;

ServerLogEntry::ServerLogEntry(const ServerLogEntry& other) = default;

ServerLogEntry::~ServerLogEntry() = default;

void ServerLogEntry::Set(const std::string& key, const std::string& value) {
  values_map_[key] = value;
}

void ServerLogEntry::AddCpuField() {
  Set(kKeyCpu, SysInfo::OperatingSystemArchitecture());
}

void ServerLogEntry::AddModeField(ServerLogEntry::Mode mode) {
  const char* mode_value = nullptr;
  switch (mode) {
    case IT2ME:
      mode_value = kValueModeIt2Me;
      break;
    case ME2ME:
      mode_value = kValueModeMe2Me;
      break;
    default:
      NOTREACHED();
  }
  Set(kKeyMode, mode_value);
}

void ServerLogEntry::AddRoleField(const char* role) {
  Set(kKeyRole, role);
}

void ServerLogEntry::AddEventNameField(const char* name) {
  Set(kKeyEventName, name);
}

// static
std::unique_ptr<XmlElement> ServerLogEntry::MakeStanza() {
  return std::make_unique<XmlElement>(
      QName(kChromotingXmlNamespace, kLogCommand));
}

std::unique_ptr<XmlElement> ServerLogEntry::ToStanza() const {
  std::unique_ptr<XmlElement> stanza(
      new XmlElement(QName(kChromotingXmlNamespace, kLogEntry)));
  ValuesMap::const_iterator iter;
  for (iter = values_map_.begin(); iter != values_map_.end(); ++iter) {
    stanza->AddAttr(QName(std::string(), iter->first), iter->second);
  }
  return stanza;
}

apis::v1::GenericLogEntry ServerLogEntry::ToGenericLogEntry() const {
  apis::v1::GenericLogEntry log_entry;
  for (auto pair : values_map_) {
    auto* field = log_entry.add_field();
    field->set_key(pair.first);
    field->set_value(pair.second);
  }
  return log_entry;
}

}  // namespace remoting
