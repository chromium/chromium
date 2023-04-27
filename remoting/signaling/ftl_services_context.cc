// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/ftl_services_context.h"

#include "base/uuid.h"
#include "build/build_config.h"
#include "google_apis/google_api_keys.h"
#include "remoting/base/service_urls.h"

namespace remoting {

namespace {

constexpr char kChromotingAppIdentifier[] = "CRD";

}  // namespace

constexpr base::TimeDelta FtlServicesContext::kBackoffInitialDelay;
constexpr base::TimeDelta FtlServicesContext::kBackoffMaxDelay;

// static
const net::BackoffEntry::Policy& FtlServicesContext::GetBackoffPolicy() {
  static const net::BackoffEntry::Policy kBackoffPolicy = {
      // Number of initial errors (in sequence) to ignore before applying
      // exponential back-off rules.
      0,

      // Initial delay for exponential back-off in ms.
      static_cast<int>(kBackoffInitialDelay.InMilliseconds()),

      // Factor by which the waiting time will be multiplied.
      2,

      // Fuzzing percentage. ex: 10% will spread requests randomly
      // between 90%-100% of the calculated time.
      0.5,

      // Maximum amount of time we are willing to delay our request in ms.
      kBackoffMaxDelay.InMilliseconds(),

      // Time to keep an entry from being discarded even when it
      // has no significant state, -1 to never discard.
      -1,

      // Starts with initial delay.
      false,
  };

  return kBackoffPolicy;
}

// static
std::string FtlServicesContext::GetServerEndpoint() {
  return ServiceUrls::GetInstance()->ftl_server_endpoint();
}

// static
std::string FtlServicesContext::GetChromotingAppIdentifier() {
  return kChromotingAppIdentifier;
}

// static
ftl::Id FtlServicesContext::CreateIdFromString(const std::string& ftl_id) {
  ftl::Id id;
  id.set_id(ftl_id);
  id.set_app(GetChromotingAppIdentifier());
  // TODO(yuweih): Migrate to IdType.Type.CHROMOTING_ID.
  id.set_type(ftl::IdType_Type_EMAIL);
  return id;
}

// static
ftl::RequestHeader FtlServicesContext::CreateRequestHeader(
    const std::string& ftl_auth_token) {
  ftl::RequestHeader header;
  header.set_request_id(base::Uuid::GenerateRandomV4().AsLowercaseString());
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
#if BUILDFLAG(IS_ANDROID)
  platform_type = ftl::Platform_Type_FTL_ANDROID;
#elif BUILDFLAG(IS_IOS)
  platform_type = ftl::Platform_Type_FTL_IOS;
#else
  platform_type = ftl::Platform_Type_FTL_DESKTOP;
#endif
  client_info->set_platform_type(platform_type);
  return header;
}

}  // namespace remoting
