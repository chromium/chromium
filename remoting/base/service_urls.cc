// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/service_urls.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "google_apis/google_api_keys.h"

// Configurable service data.
constexpr char kGcdBaseUrl[] = "https://www.googleapis.com/clouddevices/v1";
constexpr char kGcdJid[] = "clouddevices.gserviceaccount.com";
constexpr char kFtlServerEndpoint[] = "instantmessaging-pa.googleapis.com";
constexpr char kRemotingServerEndpoint[] = "remotedesktop-pa.googleapis.com";

// Command line switches.
#if !defined(NDEBUG)
constexpr char kGcdBaseUrlSwitch[] = "gcd-base-url";
constexpr char kGcdJidSwitch[] = "gcd-jid";
constexpr char kFtlServerEndpointSwitch[] = "ftl-server-endpoint";
constexpr char kRemotingServerEndpointSwitch[] = "remoting-server-endpoint";
#endif  // !defined(NDEBUG)

namespace remoting {

ServiceUrls::ServiceUrls()
    : gcd_base_url_(kGcdBaseUrl),
      gcd_jid_(kGcdJid),
      ftl_server_endpoint_(kFtlServerEndpoint),
      remoting_server_endpoint_(kRemotingServerEndpoint) {
#if !defined(NDEBUG)
  // The command line may not be initialized when running as a PNaCl plugin.
  if (base::CommandLine::InitializedForCurrentProcess()) {
    // Allow debug builds to override urls via command line.
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    CHECK(command_line);
    if (command_line->HasSwitch(kGcdBaseUrlSwitch)) {
      gcd_base_url_ = command_line->GetSwitchValueASCII(kGcdBaseUrlSwitch);
    }
    if (command_line->HasSwitch(kGcdJidSwitch)) {
      gcd_jid_ = command_line->GetSwitchValueASCII(kGcdJidSwitch);
    }
    if (command_line->HasSwitch(kFtlServerEndpointSwitch)) {
      ftl_server_endpoint_ =
          command_line->GetSwitchValueASCII(kFtlServerEndpointSwitch);
    }
    if (command_line->HasSwitch(kRemotingServerEndpointSwitch)) {
      remoting_server_endpoint_ =
          command_line->GetSwitchValueASCII(kRemotingServerEndpointSwitch);
    }
  }
#endif  // !defined(NDEBUG)
}

ServiceUrls::~ServiceUrls() = default;

ServiceUrls* remoting::ServiceUrls::GetInstance() {
  return base::Singleton<ServiceUrls>::get();
}

}  // namespace remoting
