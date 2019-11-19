// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/ftl_grpc_context.h"

#include <utility>

#include "base/guid.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "google_apis/google_api_keys.h"
#include "remoting/base/service_urls.h"
#include "third_party/grpc/src/include/grpcpp/client_context.h"

namespace remoting {

namespace {

constexpr char kChromotingAppIdentifier[] = "CRD";

static base::NoDestructor<GrpcChannelSharedPtr> g_channel_for_testing;

const net::BackoffEntry::Policy kBackoffPolicy = {
    // Number of initial errors (in sequence) to ignore before applying
    // exponential back-off rules.
    0,

    // Initial delay for exponential back-off in ms.
    FtlGrpcContext::kBackoffInitialDelay.InMilliseconds(),

    // Factor by which the waiting time will be multiplied.
    2,

    // Fuzzing percentage. ex: 10% will spread requests randomly
    // between 90%-100% of the calculated time.
    0.5,

    // Maximum amount of time we are willing to delay our request in ms.
    FtlGrpcContext::kBackoffMaxDelay.InMilliseconds(),

    // Time to keep an entry from being discarded even when it
    // has no significant state, -1 to never discard.
    -1,

    // Starts with initial delay.
    false,
};

}  // namespace

constexpr base::TimeDelta FtlGrpcContext::kBackoffInitialDelay;
constexpr base::TimeDelta FtlGrpcContext::kBackoffMaxDelay;

// static
const net::BackoffEntry::Policy& FtlGrpcContext::GetBackoffPolicy() {
  return kBackoffPolicy;
}

// static
std::string FtlGrpcContext::GetChromotingAppIdentifier() {
  return kChromotingAppIdentifier;
}

// static
ftl::Id FtlGrpcContext::CreateIdFromString(const std::string& ftl_id) {
  ftl::Id id;
  id.set_id(ftl_id);
  id.set_app(GetChromotingAppIdentifier());
  // TODO(yuweih): Migrate to IdType.Type.CHROMOTING_ID.
  id.set_type(ftl::IdType_Type_EMAIL);
  return id;
}

// static
GrpcChannelSharedPtr FtlGrpcContext::CreateChannel() {
  if (*g_channel_for_testing) {
    return *g_channel_for_testing;
  }
  return CreateSslChannelForEndpoint(
      ServiceUrls::GetInstance()->ftl_server_endpoint());
}

// static
void FtlGrpcContext::FillClientContext(grpc_impl::ClientContext* context) {
#if defined(OS_CHROMEOS)
  // Use the default Chrome API key for ChromeOS as the only host instance
  // which runs there is used for the ChromeOS Enterprise Kiosk mode
  // scenario.  If we decide to implement a remote access host for ChromeOS,
  // then we will need a way for the caller to provide an API key.
  context->AddMetadata("x-goog-api-key", google_apis::GetAPIKey());
#else
  context->AddMetadata("x-goog-api-key", google_apis::GetRemotingAPIKey());
#endif
}

// static
ftl::RequestHeader FtlGrpcContext::CreateRequestHeader(
    const std::string& ftl_auth_token) {
  ftl::RequestHeader header;
  header.set_request_id(base::GenerateGUID());
  header.set_app(kChromotingAppIdentifier);
  if (!ftl_auth_token.empty()) {
    header.set_auth_token_payload(ftl_auth_token);
  }
  ftl::ClientInfo* client_info = header.mutable_client_info();
  client_info->set_api_version(ftl::ApiVersion_Value_V4);
  client_info->set_version_major(VERSION_MAJOR);
  // Chrome's version has four number components, and the VERSION_MINOR is
  // always 0, like X.0.X.X. The FTL server requires three-component version
  // number so we just skip the VERSION_MINOR here.
  client_info->set_version_minor(VERSION_BUILD);
  client_info->set_version_point(VERSION_PATCH);
  ftl::Platform_Type platform_type;
#if defined(OS_ANDROID)
  platform_type = ftl::Platform_Type_FTL_ANDROID;
#elif defined(OS_IOS)
  platform_type = ftl::Platform_Type_FTL_IOS;
#else
  platform_type = ftl::Platform_Type_FTL_DESKTOP;
#endif
  client_info->set_platform_type(platform_type);
  return header;
}

// static
void FtlGrpcContext::SetChannelForTesting(GrpcChannelSharedPtr channel) {
  *g_channel_for_testing = channel;
}

}  // namespace remoting
