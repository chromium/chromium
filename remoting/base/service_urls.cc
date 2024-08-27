// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/service_urls.h"

#include "base/check.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "remoting/base/buildflags.h"

#if BUILDFLAG(REMOTING_INTERNAL)
#include "remoting/internal/base/service_urls.h"
#endif

// Configurable service data.
// Debug builds should default to the autopush environment (can be configured
// via cmd line switch).  Release builds will point to the prod environment.
#if defined(NDEBUG)
constexpr char kFtlServerEndpoint[] = "instantmessaging-pa.googleapis.com";
constexpr char kRemotingCloudEndpoint[] = "remotingcloud-pa.googleapis.com";
constexpr char kRemotingServerEndpoint[] = "remotedesktop-pa.googleapis.com";
#else
constexpr char kFtlServerEndpoint[] =
    "tachyon-playground-autopush-grpc.sandbox.googleapis.com";
constexpr char kRemotingCloudEndpoint[] =
    "autopush-remotingcloud-pa.sandbox.googleapis.com";
constexpr char kRemotingServerEndpoint[] =
    "autopush-remotedesktop-pa.sandbox.googleapis.com";
#endif

// Command line switches.
#if !defined(NDEBUG)
constexpr char kFtlServerEndpointSwitch[] = "ftl-server-endpoint";
constexpr char kRemotingCloudEndpointSwitch[] = "remoting-cloud-endpoint";
constexpr char kRemotingServerEndpointSwitch[] = "remoting-server-endpoint";
#endif  // !defined(NDEBUG)

namespace remoting {

ServiceUrls::ServiceUrls()
    : ftl_server_endpoint_(kFtlServerEndpoint),
      remoting_cloud_endpoint_(kRemotingCloudEndpoint),
      remoting_server_endpoint_(kRemotingServerEndpoint) {
#if BUILDFLAG(REMOTING_INTERNAL)
  remoting_corp_endpoint_ = internal::GetRemotingCorpApiUrl();
#endif

#if !defined(NDEBUG)
  // The command line may not be initialized when running as a PNaCl plugin.
  if (base::CommandLine::InitializedForCurrentProcess()) {
    // Allow debug builds to override urls via command line.
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    CHECK(command_line);
    if (command_line->HasSwitch(kFtlServerEndpointSwitch)) {
      ftl_server_endpoint_ =
          command_line->GetSwitchValueASCII(kFtlServerEndpointSwitch);
    } else {
      LOG(WARNING) << "CRD: Using autopush (non prod) FTL server";
    }
    if (command_line->HasSwitch(kRemotingCloudEndpointSwitch)) {
      remoting_cloud_endpoint_ =
          command_line->GetSwitchValueASCII(kRemotingCloudEndpointSwitch);
    } else {
      LOG(WARNING) << "CRD: Using autopush (non prod) remoting cloud API";
    }
    if (command_line->HasSwitch(kRemotingServerEndpointSwitch)) {
      remoting_server_endpoint_ =
          command_line->GetSwitchValueASCII(kRemotingServerEndpointSwitch);
    } else {
      LOG(WARNING) << "CRD: Using autopush (non prod) remoting server";
    }

#if BUILDFLAG(REMOTING_INTERNAL)
    const char kRemotingCorpEndpointSwitch[] = "remoting-corp-endpoint";
    if (command_line->HasSwitch(kRemotingCorpEndpointSwitch)) {
      remoting_corp_endpoint_ =
          command_line->GetSwitchValueASCII(kRemotingCorpEndpointSwitch);
    } else {
      LOG(WARNING) << "CRD: Using autopush (non prod) remotedesktop Corp API";
    }
#endif  // BUILDFLAG(REMOTING_INTERNAL)
  }
#endif  // !defined(NDEBUG)
}

ServiceUrls::~ServiceUrls() = default;

ServiceUrls* remoting::ServiceUrls::GetInstance() {
  return base::Singleton<ServiceUrls>::get();
}

}  // namespace remoting
