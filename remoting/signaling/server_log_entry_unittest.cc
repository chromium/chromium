// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/server_log_entry_unittest.h"

#include <sstream>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

using jingle_xmpp::QName;
using jingle_xmpp::XmlAttr;
using jingle_xmpp::XmlElement;

namespace remoting {

const char kJabberClientNamespace[] = "jabber:client";
const char kChromotingNamespace[] = "google:remoting";

XmlElement* GetLogElementFromStanza(XmlElement* stanza) {
  if (stanza->Name() != QName(kJabberClientNamespace, "iq")) {
    ADD_FAILURE() << "Expected element 'iq'";
    return nullptr;
  }
  XmlElement* log_element = stanza->FirstChild()->AsElement();
  if (log_element->Name() != QName(kChromotingNamespace, "log")) {
    ADD_FAILURE() << "Expected element 'log'";
    return nullptr;
  }
  if (log_element->NextChild()) {
    ADD_FAILURE() << "Expected only 1 child of 'iq'";
    return nullptr;
  }
  return log_element;
}

XmlElement* GetSingleLogEntryFromStanza(XmlElement* stanza) {
  XmlElement* log_element = GetLogElementFromStanza(stanza);
  if (!log_element) {
    // Test failure already recorded, so just return nullptr here.
    return nullptr;
  }
  XmlElement* entry = log_element->FirstChild()->AsElement();
  if (entry->Name() != QName(kChromotingNamespace, "entry")) {
    ADD_FAILURE() << "Expected element 'entry'";
    return nullptr;
  }
  if (entry->NextChild()) {
    ADD_FAILURE() << "Expected only 1 child of 'log'";
    return nullptr;
  }
  return entry;
}

bool VerifyStanza(const std::map<std::string, std::string>& key_value_pairs,
                  const std::set<std::string> keys,
                  const XmlElement* elem,
                  std::string* error) {
  int attrCount = 0;
  for (const XmlAttr* attr = elem->FirstAttr(); attr != nullptr;
       attr = attr->NextAttr(), attrCount++) {
    if (attr->Name().Namespace().length() != 0) {
      *error = "attribute has non-empty namespace " + attr->Name().Namespace();
      return false;
    }
    const std::string& key = attr->Name().LocalPart();
    const std::string& value = attr->Value();
    auto iter = key_value_pairs.find(key);
    if (iter == key_value_pairs.end()) {
      if (keys.find(key) == keys.end()) {
        *error = "unexpected attribute " + key;
        return false;
      }
    } else {
      if (iter->second != value) {
        *error = "attribute " + key + " has value " + iter->second +
                 ": expected " + value;
        return false;
      }
    }
  }
  int attr_count_expected = key_value_pairs.size() + keys.size();
  if (attrCount != attr_count_expected) {
    std::stringstream s;
    s << "stanza has " << attrCount << " keys: expected "
      << attr_count_expected;
    *error = s.str();
    return false;
  }
  return true;
}

}  // namespace remoting
