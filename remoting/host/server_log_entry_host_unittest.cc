// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/server_log_entry_host.h"

#include <memory>

#include "base/strings/stringize_macros.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "remoting/signaling/server_log_entry.h"
#include "remoting/signaling/server_log_entry_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

using jingle_xmpp::XmlAttr;
using jingle_xmpp::XmlElement;

namespace remoting {

TEST(ServerLogEntryHostTest, MakeForSessionStateChange) {
  std::unique_ptr<ServerLogEntry> entry(
      MakeLogEntryForSessionStateChange(true));
  std::unique_ptr<XmlElement> stanza = entry->ToStanza();
  std::string error;
  std::map<std::string, std::string> key_value_pairs;
  key_value_pairs["role"] = "host";
  key_value_pairs["event-name"] = "session-state";
  key_value_pairs["session-state"] = "connected";
  std::set<std::string> keys;
  ASSERT_TRUE(VerifyStanza(key_value_pairs, keys, stanza.get(), &error))
      << error;
}

TEST(ServerLogEntryHostTest, AddHostFields) {
  std::unique_ptr<ServerLogEntry> entry(
      MakeLogEntryForSessionStateChange(true));
  AddHostFieldsToLogEntry(entry.get());
  std::unique_ptr<XmlElement> stanza = entry->ToStanza();
  std::string error;
  std::map<std::string, std::string> key_value_pairs;
  key_value_pairs["role"] = "host";
  key_value_pairs["event-name"] = "session-state";
  key_value_pairs["session-state"] = "connected";
  std::set<std::string> keys;
  keys.insert("cpu");
#if BUILDFLAG(IS_WIN)
  key_value_pairs["os-name"] = "Windows";
  keys.insert("os-version");
#elif BUILDFLAG(IS_APPLE)
  key_value_pairs["os-name"] = "Mac";
  keys.insert("os-version");
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  key_value_pairs["os-name"] = "ChromeOS";
  keys.insert("os-version");
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  key_value_pairs["os-name"] = "Linux";
  keys.insert("os-version");
#endif

// The check below will compile but fail if VERSION isn't defined (STRINGIZE
// silently converts undefined values).
#ifndef VERSION
#error VERSION must be defined
#endif
  key_value_pairs["host-version"] = STRINGIZE(VERSION);
  ASSERT_TRUE(VerifyStanza(key_value_pairs, keys, stanza.get(), &error))
      << error;
}

TEST(ServerLogEntryHostTest, AddModeField1) {
  std::unique_ptr<ServerLogEntry> entry(
      MakeLogEntryForSessionStateChange(true));
  entry->AddModeField(ServerLogEntry::IT2ME);
  std::unique_ptr<XmlElement> stanza = entry->ToStanza();
  std::string error;
  std::map<std::string, std::string> key_value_pairs;
  key_value_pairs["role"] = "host";
  key_value_pairs["event-name"] = "session-state";
  key_value_pairs["session-state"] = "connected";
  key_value_pairs["mode"] = "it2me";
  std::set<std::string> keys;
  ASSERT_TRUE(VerifyStanza(key_value_pairs, keys, stanza.get(), &error))
      << error;
}

TEST(ServerLogEntryHostTest, AddModeField2) {
  std::unique_ptr<ServerLogEntry> entry(
      MakeLogEntryForSessionStateChange(true));
  entry->AddModeField(ServerLogEntry::ME2ME);
  std::unique_ptr<XmlElement> stanza = entry->ToStanza();
  std::string error;
  std::map<std::string, std::string> key_value_pairs;
  key_value_pairs["role"] = "host";
  key_value_pairs["event-name"] = "session-state";
  key_value_pairs["session-state"] = "connected";
  key_value_pairs["mode"] = "me2me";
  std::set<std::string> keys;
  ASSERT_TRUE(VerifyStanza(key_value_pairs, keys, stanza.get(), &error))
      << error;
}

}  // namespace remoting
