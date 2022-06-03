// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/service_urls.h"

#include "base/check.h"
#include "base/command_line.h"
#include "google_apis/google_api_keys.h"

// Configurable service data.
// Debug builds should default to the autopush environment (can be configured
// via cmd line switch).  Release builds will point to the prod environment.
#if defined(NDEBUG)
constexpr char kFtlServerEndpoint[] = "instantmessaging-pa.googleapis.com";
constexpr char kRemotingServerEndpoint[] = "remotedesktop-pa.googleapis.com";
#else
constexpr char kFtlServerEndpoint[] =
    "tachyon-playground-autopush-grpc.sandbox.googleapis.com";
constexpr char kRemotingServerEndpoint[] =
    "autopush-remotedesktop-pa.sandbox.googleapis.com";
#endif

// Command line switches.
#if !defined(NDEBUG)
constexpr char kFtlServerEndpointSwitch[] = "ftl-server-endpoint";
constexpr char kRemotingServerEndpointSwitch[] = "remoting-server-endpoint";
#endif  // !defined(NDEBUG)

namespace remoting {

ServiceUrls::ServiceUrls()
    : ftl_server_endpoint_(kFtlServerEndpoint),
      remoting_server_endpoint_(kRemotingServerEndpoint) {
#if !defined(NDEBUG)
  // The command line may not be initialized when running as a PNaCl plugin.
  if (base::CommandLine::InitializedForCurrentProcess()) {
    // Allow debug builds to override urls via command line.
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    CHECK(command_line);
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
