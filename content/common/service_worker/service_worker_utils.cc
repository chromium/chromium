// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_utils.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/numerics/safe_math.h"
#include "base/strings/string_util.h"
#include "content/common/service_worker/service_worker_types.pb.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/origin_util.h"
#include "net/base/load_flags.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/features.h"

namespace content {

namespace {

bool PathContainsDisallowedCharacter(const GURL& url) {
  std::string path = url.path();
  DCHECK(base::IsStringUTF8(path));

  // We should avoid these escaped characters in the path component because
  // these can be handled differently depending on server implementation.
  if (path.find("%2f") != std::string::npos ||
      path.find("%2F") != std::string::npos) {
    return true;
  }
  if (path.find("%5c") != std::string::npos ||
      path.find("%5C") != std::string::npos) {
    return true;
  }
  return false;
}

}  // namespace

// static
bool ServiceWorkerUtils::ScopeMatches(const GURL& scope, const GURL& url) {
  DCHECK(!scope.has_ref());
  return base::StartsWith(url.spec(), scope.spec(),
                          base::CompareCase::SENSITIVE);
}

// static
bool ServiceWorkerUtils::IsPathRestrictionSatisfied(
    const GURL& scope,
    const GURL& script_url,
    const std::string* service_worker_allowed_header_value,
    std::string* error_message) {
  return IsPathRestrictionSatisfiedInternal(scope, script_url, true,
                                            service_worker_allowed_header_value,
                                            error_message);
}

// static
bool ServiceWorkerUtils::IsPathRestrictionSatisfiedWithoutHeader(
    const GURL& scope,
    const GURL& script_url,
    std::string* error_message) {
  return IsPathRestrictionSatisfiedInternal(scope, script_url, false, nullptr,
                                            error_message);
}

// static
bool ServiceWorkerUtils::IsPathRestrictionSatisfiedInternal(
    const GURL& scope,
    const GURL& script_url,
    bool service_worker_allowed_header_supported,
    const std::string* service_worker_allowed_header_value,
    std::string* error_message) {
  DCHECK(scope.is_valid());
  DCHECK(!scope.has_ref());
  DCHECK(script_url.is_valid());
  DCHECK(!script_url.has_ref());
  DCHECK(error_message);

  if (ContainsDisallowedCharacter(scope, script_url, error_message))
    return false;

  std::string max_scope_string;
  if (service_worker_allowed_header_value &&
      service_worker_allowed_header_supported) {
    GURL max_scope = script_url.Resolve(*service_worker_allowed_header_value);
    if (!max_scope.is_valid()) {
      *error_message = "An invalid Service-Worker-Allowed header value ('";
      error_message->append(*service_worker_allowed_header_value);
      error_message->append("') was received when fetching the script.");
      return false;
    }
    max_scope_string = max_scope.path();
  } else {
    max_scope_string = script_url.GetWithoutFilename().path();
  }

  std::string scope_string = scope.path();
  if (!base::StartsWith(scope_string, max_scope_string,
                        base::CompareCase::SENSITIVE)) {
    *error_message = "The path of the provided scope ('";
    error_message->append(scope_string);
    error_message->append("') is not under the max scope allowed (");
    if (service_worker_allowed_header_value &&
        service_worker_allowed_header_supported)
      error_message->append("set by Service-Worker-Allowed: ");
    error_message->append("'");
    error_message->append(max_scope_string);
    if (service_worker_allowed_header_supported) {
      error_message->append(
          "'). Adjust the scope, move the Service Worker script, or use the "
          "Service-Worker-Allowed HTTP header to allow the scope.");
    } else {
      error_message->append(
          "'). Adjust the scope or move the Service Worker script.");
    }
    return false;
  }
  return true;
}

// static
bool ServiceWorkerUtils::ContainsDisallowedCharacter(
    const GURL& scope,
    const GURL& script_url,
    std::string* error_message) {
  if (PathContainsDisallowedCharacter(scope) ||
      PathContainsDisallowedCharacter(script_url)) {
    *error_message = "The provided scope ('";
    error_message->append(scope.spec());
    error_message->append("') or scriptURL ('");
    error_message->append(script_url.spec());
    error_message->append("') includes a disallowed escape character.");
    return true;
  }
  return false;
}

// static
bool ServiceWorkerUtils::AllOriginsMatchAndCanAccessServiceWorkers(
    const std::vector<GURL>& urls) {
  // (A) Check if all origins can access service worker. Every URL must be
  // checked despite the same-origin check below in (B), because GetOrigin()
  // uses the inner URL for filesystem URLs so that https://foo/ and
  // filesystem:https://foo/ are considered equal, but filesystem URLs cannot
  // access service worker.
  for (const GURL& url : urls) {
    if (!OriginCanAccessServiceWorkers(url))
      return false;
  }

  // (B) Check if all origins are equal. Cross-origin access is permitted when
  // --disable-web-security is set.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableWebSecurity)) {
    return true;
  }
  const GURL& first = urls.front();
  for (const GURL& url : urls) {
    if (first.GetOrigin() != url.GetOrigin())
      return false;
  }
  return true;
}

bool ServiceWorkerUtils::ShouldBypassCacheDueToUpdateViaCache(
    bool is_main_script,
    blink::mojom::ServiceWorkerUpdateViaCache cache_mode) {
  switch (cache_mode) {
    case blink::mojom::ServiceWorkerUpdateViaCache::kImports:
      return is_main_script;
    case blink::mojom::ServiceWorkerUpdateViaCache::kNone:
      return true;
    case blink::mojom::ServiceWorkerUpdateViaCache::kAll:
      return false;
  }
  NOTREACHED() << static_cast<int>(cache_mode);
  return false;
}

// static
blink::mojom::FetchCacheMode ServiceWorkerUtils::GetCacheModeFromLoadFlags(
    int load_flags) {
  if (load_flags & net::LOAD_DISABLE_CACHE)
    return blink::mojom::FetchCacheMode::kNoStore;

  if (load_flags & net::LOAD_VALIDATE_CACHE)
    return blink::mojom::FetchCacheMode::kValidateCache;

  if (load_flags & net::LOAD_BYPASS_CACHE) {
    if (load_flags & net::LOAD_ONLY_FROM_CACHE)
      return blink::mojom::FetchCacheMode::kUnspecifiedForceCacheMiss;
    return blink::mojom::FetchCacheMode::kBypassCache;
  }

  if (load_flags & net::LOAD_SKIP_CACHE_VALIDATION) {
    if (load_flags & net::LOAD_ONLY_FROM_CACHE)
      return blink::mojom::FetchCacheMode::kOnlyIfCached;
    return blink::mojom::FetchCacheMode::kForceCache;
  }

  if (load_flags & net::LOAD_ONLY_FROM_CACHE) {
    DCHECK(!(load_flags & net::LOAD_SKIP_CACHE_VALIDATION));
    DCHECK(!(load_flags & net::LOAD_BYPASS_CACHE));
    return blink::mojom::FetchCacheMode::kUnspecifiedOnlyIfCachedStrict;
  }
  return blink::mojom::FetchCacheMode::kDefault;
}

// static
std::string ServiceWorkerUtils::SerializeFetchRequestToString(
    const ServiceWorkerFetchRequest& request) {
  proto::internal::ServiceWorkerFetchRequest request_proto;

  request_proto.set_url(request.url.spec());
  request_proto.set_method(request.method);
  request_proto.mutable_headers()->insert(request.headers.begin(),
                                          request.headers.end());
  request_proto.mutable_referrer()->set_url(request.referrer.url.spec());
  request_proto.mutable_referrer()->set_policy(
      static_cast<int>(request.referrer.policy));
  request_proto.set_is_reload(request.is_reload);
  request_proto.set_mode(static_cast<int>(request.mode));
  request_proto.set_is_main_resource_load(request.is_main_resource_load);
  request_proto.set_request_context_type(
      static_cast<int>(request.request_context_type));
  request_proto.set_credentials_mode(
      static_cast<int>(request.credentials_mode));
  request_proto.set_cache_mode(static_cast<int>(request.cache_mode));
  request_proto.set_redirect_mode(static_cast<int>(request.redirect_mode));
  request_proto.set_integrity(request.integrity);
  request_proto.set_keepalive(request.keepalive);
  request_proto.set_is_history_navigation(request.is_history_navigation);
  request_proto.set_client_id(request.client_id);

  return request_proto.SerializeAsString();
}

// static
ServiceWorkerFetchRequest ServiceWorkerUtils::DeserializeFetchRequestFromString(
    const std::string& serialized) {
  proto::internal::ServiceWorkerFetchRequest request_proto;
  if (!request_proto.ParseFromString(serialized)) {
    return ServiceWorkerFetchRequest();
  }

  ServiceWorkerFetchRequest request(
      GURL(request_proto.url()), request_proto.method(),
      ServiceWorkerHeaderMap(request_proto.headers().begin(),
                             request_proto.headers().end()),
      Referrer(GURL(request_proto.referrer().url()),
               static_cast<network::mojom::ReferrerPolicy>(
                   request_proto.referrer().policy())),
      request_proto.is_reload());
  request.mode =
      static_cast<network::mojom::FetchRequestMode>(request_proto.mode());
  request.is_main_resource_load = request_proto.is_main_resource_load();
  request.request_context_type = static_cast<blink::mojom::RequestContextType>(
      request_proto.request_context_type());
  request.credentials_mode = static_cast<network::mojom::FetchCredentialsMode>(
      request_proto.credentials_mode());
  request.cache_mode =
      static_cast<blink::mojom::FetchCacheMode>(request_proto.cache_mode());
  request.redirect_mode = static_cast<network::mojom::FetchRedirectMode>(
      request_proto.redirect_mode());
  request.integrity = request_proto.integrity();
  request.keepalive = request_proto.keepalive();
  request.is_history_navigation = request_proto.is_history_navigation();
  request.client_id = request_proto.client_id();

  return request;
}

bool LongestScopeMatcher::MatchLongest(const GURL& scope) {
  if (!ServiceWorkerUtils::ScopeMatches(scope, url_))
    return false;
  if (match_.is_empty() || match_.spec().size() < scope.spec().size()) {
    match_ = scope;
    return true;
  }
  return false;
}

}  // namespace content
