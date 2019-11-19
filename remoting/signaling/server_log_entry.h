// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_SERVER_LOG_ENTRY_H_
#define REMOTING_SIGNALING_SERVER_LOG_ENTRY_H_

#include <map>
#include <memory>
#include <string>

#include "remoting/proto/remoting/v1/generic_log_entry.pb.h"

namespace jingle_xmpp {
class XmlElement;
}  // namespace jingle_xmpp

namespace remoting {

// Utility class for building log entries to send to the remoting bot. This is
// a wrapper around a key/value map and is copyable so it can be used in STL
// containers.
class ServerLogEntry {
 public:
  // The mode of a connection.
  enum Mode {
    IT2ME,
    ME2ME
  };

  ServerLogEntry();
  ServerLogEntry(const ServerLogEntry& other);
  ~ServerLogEntry();

  // Sets an arbitrary key/value entry.
  void Set(const std::string& key, const std::string& value);

  // Adds a field describing the CPU type of the platform.
  void AddCpuField();

  // Adds a field describing the mode of a connection to this log entry.
  void AddModeField(Mode mode);

  // Adds a field describing the role (client/host).
  void AddRoleField(const char* role);

  // Adds a field describing the type of log entry.
  void AddEventNameField(const char* name);

  // Constructs a log stanza. The caller should add one or more log entry
  // stanzas as children of this stanza, before sending the log stanza to
  // the remoting bot.
  static std::unique_ptr<jingle_xmpp::XmlElement> MakeStanza();

  // Converts this object to an XML stanza.
  std::unique_ptr<jingle_xmpp::XmlElement> ToStanza() const;

  apis::v1::GenericLogEntry ToGenericLogEntry() const;

 private:
  typedef std::map<std::string, std::string> ValuesMap;

  ValuesMap values_map_;
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_SERVER_LOG_ENTRY_H_
