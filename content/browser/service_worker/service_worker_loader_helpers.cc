// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_loader_helpers.h"

#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/loader/browser_initiated_resource_request.h"
#include "content/browser/service_worker/service_worker_consts.h"
#include "content/common/features.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/referrer.h"
#include "services/network/public/cpp/constants.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/common/service_worker/service_worker_scope_match.h"
#include "third_party/blink/public/mojom/loader/fetch_client_settings_object.mojom.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

namespace content {

namespace service_worker_loader_helpers {

namespace {

bool IsPathRestrictionSatisfiedInternal(
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

  if (blink::ServiceWorkerScopeOrScriptUrlContainsDisallowedCharacter(
          scope, script_url, error_message)) {
    return false;
  }

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

    if (max_scope.DeprecatedGetOriginAsURL() !=
        script_url.DeprecatedGetOriginAsURL()) {
      *error_message = "A cross-origin Service-Worker-Allowed header value ('";
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

}  // namespace

bool CheckResponseHead(
    const network::mojom::URLResponseHead& response_head,
    blink::ServiceWorkerStatusCode* out_service_worker_status,
    network::URLLoaderCompletionStatus* out_completion_status,
    std::string* out_error_message) {
  if (response_head.headers->response_code() / 100 != 2) {
    // Non-2XX HTTP status code is handled as an error.
    *out_completion_status =
        network::URLLoaderCompletionStatus(net::ERR_INVALID_RESPONSE);
    *out_error_message = base::StringPrintf(
        ServiceWorkerConsts::kServiceWorkerBadHTTPResponseError,
        response_head.headers->response_code());
    *out_service_worker_status = blink::ServiceWorkerStatusCode::kErrorNetwork;
    return false;
  }

  if (!devtools_instrumentation::ShouldBypassCertificateErrors() &&
      net::IsCertStatusError(response_head.cert_status) &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kIgnoreCertificateErrors)) {
    *out_completion_status = network::URLLoaderCompletionStatus(
        net::MapCertStatusToNetError(response_head.cert_status));
    *out_error_message = ServiceWorkerConsts::kServiceWorkerSSLError;
    *out_service_worker_status = blink::ServiceWorkerStatusCode::kErrorNetwork;
    return false;
  }

  // Remain consistent with logic in
  // blink::InstalledServiceWorkerModuleScriptFetcher::Fetch()
  if (!blink::IsSupportedJavascriptMimeType(response_head.mime_type)) {
    *out_completion_status =
        network::URLLoaderCompletionStatus(net::ERR_INSECURE_RESPONSE);
    *out_error_message =
        response_head.mime_type.empty()
            ? ServiceWorkerConsts::kServiceWorkerNoMIMEError
            : base::StringPrintf(
                  ServiceWorkerConsts::kServiceWorkerBadMIMEError,
                  response_head.mime_type.c_str());
    *out_service_worker_status = blink::ServiceWorkerStatusCode::kErrorSecurity;
    return false;
  }

  return true;
}

bool ShouldBypassCacheDueToUpdateViaCache(
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
  NOTREACHED_IN_MIGRATION() << static_cast<int>(cache_mode);
  return false;
}

bool ShouldValidateBrowserCacheForScript(
    bool is_main_script,
    bool force_bypass_cache,
    blink::mojom::ServiceWorkerUpdateViaCache cache_mode,
    base::TimeDelta time_since_last_check) {
  return (ShouldBypassCacheDueToUpdateViaCache(is_main_script, cache_mode) ||
          time_since_last_check >
              ServiceWorkerConsts::kServiceWorkerScriptMaxCacheAge ||
          force_bypass_cache);
}

#if DCHECK_IS_ON()
void CheckVersionStatusBeforeWorkerScriptLoad(
    ServiceWorkerVersion::Status status,
    bool is_main_script,
    blink::mojom::ScriptType script_type) {
  if (is_main_script) {
    // The service worker main script should be fetched during worker startup.
    DCHECK_EQ(status, ServiceWorkerVersion::NEW);
    return;
  }

  // Non-main scripts are fetched by importScripts() for classic scripts or
  // static-import for module scripts.
  switch (script_type) {
    case blink::mojom::ScriptType::kClassic:
      // importScripts() should be called until completion of the install event.
      DCHECK(status == ServiceWorkerVersion::NEW ||
             status == ServiceWorkerVersion::INSTALLING);
      break;
    case blink::mojom::ScriptType::kModule:
      // Static-import should be handled during worker startup along with the
      // main script.
      DCHECK_EQ(status, ServiceWorkerVersion::NEW);
      break;
  }
}
#endif  // DCHECK_IS_ON()

network::ResourceRequest CreateRequestForServiceWorkerScript(
    const GURL& script_url,
    const blink::StorageKey& storage_key,
    bool is_main_script,
    blink::mojom::ScriptType worker_script_type,
    const blink::mojom::FetchClientSettingsObject& fetch_client_settings_object,
    BrowserContext& browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  network::ResourceRequest request;
  request.url = script_url;

  request.site_for_cookies = storage_key.ToNetSiteForCookies();
  request.do_not_prompt_for_login = true;

  blink::RendererPreferences renderer_preferences;
  GetContentClient()->browser()->UpdateRendererPreferencesForWorker(
      &browser_context, &renderer_preferences);
  UpdateAdditionalHeadersForBrowserInitiatedRequest(
      &request.headers, &browser_context,
      /*should_update_existing_headers=*/false, renderer_preferences,
      /*is_for_worker_script=*/true);

  // Set the accept header to '*/*'.
  // https://fetch.spec.whatwg.org/#concept-fetch
  request.headers.SetHeader(net::HttpRequestHeaders::kAccept,
                            network::kDefaultAcceptHeaderValue);

  request.referrer_policy = Referrer::ReferrerPolicyForUrlRequest(
      fetch_client_settings_object.referrer_policy);
  request.referrer =
      Referrer::SanitizeForRequest(
          script_url, Referrer(fetch_client_settings_object.outgoing_referrer,
                               fetch_client_settings_object.referrer_policy))
          .url;
  request.upgrade_if_insecure =
      fetch_client_settings_object.insecure_requests_policy ==
      blink::mojom::InsecureRequestsPolicy::kUpgrade;

  const url::Origin& origin = storage_key.origin();

  // ResourceRequest::request_initiator is the request's origin in the spec.
  // https://fetch.spec.whatwg.org/#concept-request-origin
  // It's needed to be set to the origin where the service worker is registered.
  // https://github.com/w3c/ServiceWorker/issues/1447
  request.request_initiator = origin;

  // This key is used to isolate requests from different contexts in accessing
  // shared network resources like the http cache.
  request.trusted_params = network::ResourceRequest::TrustedParams();
  request.trusted_params->isolation_info =
      storage_key.ToPartialNetIsolationInfo();

  if (worker_script_type == blink::mojom::ScriptType::kClassic) {
    if (is_main_script) {
      // Set the "Service-Worker" header for the service worker script request:
      // https://w3c.github.io/ServiceWorker/#service-worker-script-request
      request.headers.SetHeader("Service-Worker", "script");

      // The "Fetch a classic worker script" uses "same-origin" as mode and
      // credentials mode.
      // https://html.spec.whatwg.org/C/#fetch-a-classic-worker-script
      request.mode = network::mojom::RequestMode::kSameOrigin;
      request.credentials_mode = network::mojom::CredentialsMode::kSameOrigin;

      // The request's destination is "serviceworker" for the main script.
      // https://w3c.github.io/ServiceWorker/#update-algorithm
      request.destination = network::mojom::RequestDestination::kServiceWorker;
      request.resource_type =
          static_cast<int>(blink::mojom::ResourceType::kServiceWorker);
    } else {
      // The "fetch a classic worker-imported script" doesn't have any statement
      // about mode and credentials mode. Use the default value, which is
      // "no-cors".
      // https://html.spec.whatwg.org/C/#fetch-a-classic-worker-imported-script
      DCHECK_EQ(network::mojom::RequestMode::kNoCors, request.mode);

      // The request's destination is "script" for the imported script.
      // https://w3c.github.io/ServiceWorker/#update-algorithm
      request.destination = network::mojom::RequestDestination::kScript;
      request.resource_type =
          static_cast<int>(blink::mojom::ResourceType::kScript);
    }
  } else {
    if (is_main_script) {
      // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-module-worker-script-tree
      // Set the "Service-Worker" header for the service worker script request:
      // https://w3c.github.io/ServiceWorker/#service-worker-script-request
      request.headers.SetHeader("Service-Worker", "script");

      // The "Fetch a module worker script graph" uses "same-origin" as mode for
      // main script and "cors" otherwise.
      // https://w3c.github.io/ServiceWorker/#update-algorithm
      request.mode = network::mojom::RequestMode::kSameOrigin;
    } else {
      request.mode = network::mojom::RequestMode::kCors;
    }

    // The "Fetch a module worker script graph" uses "omit" as credentials
    // mode.
    // https://w3c.github.io/ServiceWorker/#update-algorithm
    request.credentials_mode = network::mojom::CredentialsMode::kOmit;

    // The request's destination is "serviceworker" for the main and
    // static-imported module script.
    // https://w3c.github.io/ServiceWorker/#update-algorithm
    request.destination = network::mojom::RequestDestination::kServiceWorker;
    request.resource_type =
        static_cast<int>(blink::mojom::ResourceType::kServiceWorker);
  }

  return request;
}

bool IsPathRestrictionSatisfied(
    const GURL& scope,
    const GURL& script_url,
    const std::string* service_worker_allowed_header_value,
    std::string* error_message) {
  return IsPathRestrictionSatisfiedInternal(scope, script_url, true,
                                            service_worker_allowed_header_value,
                                            error_message);
}

bool IsPathRestrictionSatisfiedWithoutHeader(const GURL& scope,
                                             const GURL& script_url,
                                             std::string* error_message) {
  return IsPathRestrictionSatisfiedInternal(scope, script_url, false, nullptr,
                                            error_message);
}

const base::flat_set<std::string> FetchHandlerBypassedHashStrings() {
  const static base::NoDestructor<base::flat_set<std::string>> result(
      base::SplitString(
          features::kServiceWorkerBypassFetchHandlerBypassedHashStrings.Get(),
          ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY));

  return *result;
}

}  // namespace service_worker_loader_helpers

}  // namespace content
