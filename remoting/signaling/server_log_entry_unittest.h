// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_SERVER_LOG_ENTRY_UNITTEST_H_
#define REMOTING_SIGNALING_SERVER_LOG_ENTRY_UNITTEST_H_

#include <map>
#include <set>
#include <string>

namespace jingle_xmpp {
class XmlElement;
}  // namespace jingle_xmpp

namespace remoting {

extern const char kJabberClientNamespace[];
extern const char kChromotingNamespace[];

// Verifies that |stanza| contains a <log> element and returns it. Otherwise
// returns nullptr and records a test failure.
jingle_xmpp::XmlElement* GetLogElementFromStanza(
    jingle_xmpp::XmlElement* stanza);

// Verifies that |stanza| contains only 1 log entry, and returns the <entry>
// element. Otherwise returns nullptr and records a test failure.
jingle_xmpp::XmlElement* GetSingleLogEntryFromStanza(
    jingle_xmpp::XmlElement* stanza);

// Verifies a logging stanza.
// |keyValuePairs| lists the keys that must have specified values, and |keys|
// lists the keys that must be present, but may have arbitrary values.
// There must be no other keys.
bool VerifyStanza(const std::map<std::string, std::string>& key_value_pairs,
                  const std::set<std::string> keys,
                  const jingle_xmpp::XmlElement* elem,
                  std::string* error);

}  // namespace remoting

#endif  // REMOTING_SIGNALING_SERVER_LOG_ENTRY_UNITTEST_H_
