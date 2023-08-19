// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/web_bundle/script_web_bundle_rule.h"

#include "base/containers/contains.h"
#include "base/metrics/histogram_macros.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/renderer/platform/json/json_parser.h"
#include "third_party/blink/renderer/platform/json/json_values.h"

namespace blink {

namespace {

const char kSourceKey[] = "source";
const char kCredentialsKey[] = "credentials";
const char kScopesKey[] = "scopes";
const char kResourcesKey[] = "resources";
const char* const kKnownKeys[] = {kSourceKey, kCredentialsKey, kScopesKey,
                                  kResourcesKey};

HashSet<KURL> ParseJSONArrayAsURLs(JSONArray* array, const KURL& base_url) {
  HashSet<KURL> urls;
  if (!array)
    return urls;
  for (wtf_size_t i = 0; i < array->size(); ++i) {
    String relative_url;
    if (array->at(i)->AsString(&relative_url)) {
      KURL url(base_url, relative_url);
      if (url.IsValid()) {
        urls.insert(url);
      }
    }
  }
  return urls;
}

network::mojom::CredentialsMode ParseCredentials(const String& credentials) {
  if (credentials == "omit")
    return network::mojom::CredentialsMode::kOmit;
  if (credentials == "same-origin")
    return network::mojom::CredentialsMode::kSameOrigin;
  if (credentials == "include")
    return network::mojom::CredentialsMode::kInclude;
  // The default is "same-origin".
  return network::mojom::CredentialsMode::kSameOrigin;
}

}  // namespace

absl::variant<ScriptWebBundleRule, ScriptWebBundleError>
ScriptWebBundleRule::ParseJson(const String& inline_text,
                               const KURL& base_url,
                               ConsoleLogger* logger) {
  std::unique_ptr<JSONValue> json = ParseJSON(inline_text);
  if (!json) {
    return ScriptWebBundleError(
        ScriptWebBundleError::Type::kSyntaxError,
        "Failed to parse web bundle rule: invalid JSON.");
  }
  std::unique_ptr<JSONObject> json_obj = JSONObject::From(std::move(json));
  if (!json_obj) {
    return ScriptWebBundleError(
        ScriptWebBundleError::Type::kTypeError,
        "Failed to parse web bundle rule: not an object.");
  }

  // Emit console warning for unknown keys.
  if (logger) {
    for (wtf_size_t i = 0; i < json_obj->size(); ++i) {
      JSONObject::Entry entry = json_obj->at(i);
      if (!base::Contains(kKnownKeys, entry.first)) {
        logger->AddConsoleMessage(
            mojom::blink::ConsoleMessageSource::kOther,
            mojom::blink::ConsoleMessageLevel::kWarning,
            "Invalid top-level key \"" + entry.first + "\" in WebBundle rule.");
      }
    }
  }

  String source;
  if (!json_obj->GetString(kSourceKey, &source)) {
    return ScriptWebBundleError(ScriptWebBundleError::Type::kTypeError,
                                "Failed to parse web bundle rule: \"source\" "
                                "top-level key must be a string.");
  }
  KURL source_url(base_url, source);
  if (!source_url.IsValid()) {
    return ScriptWebBundleError(ScriptWebBundleError::Type::kTypeError,
                                "Failed to parse web bundle rule: \"source\" "
                                "is not parsable as a URL.");
  }

  network::mojom::CredentialsMode credentials_mode;
  String credentials;
  if (json_obj->GetString(kCredentialsKey, &credentials)) {
    credentials_mode = ParseCredentials(credentials);
  } else {
    // The default is "same-origin".
    credentials_mode = network::mojom::CredentialsMode::kSameOrigin;
  }

  JSONValue* scopes = json_obj->Get(kScopesKey);
  if (scopes && scopes->GetType() != JSONValue::kTypeArray) {
    return ScriptWebBundleError(
        ScriptWebBundleError::Type::kTypeError,
        "Failed to parse web bundle rule: \"scopes\" must be an array.");
  }
  JSONValue* resources = json_obj->Get(kResourcesKey);
  if (resources && resources->GetType() != JSONValue::kTypeArray) {
    return ScriptWebBundleError(
        ScriptWebBundleError::Type::kTypeError,
        "Failed to parse web bundle rule: \"resources\" must be an array.");
  }

  HashSet<KURL> scope_urls =
      ParseJSONArrayAsURLs(JSONArray::Cast(scopes), source_url);
  HashSet<KURL> resource_urls =
      ParseJSONArrayAsURLs(JSONArray::Cast(resources), source_url);

  return ScriptWebBundleRule(source_url, credentials_mode,
                             std::move(scope_urls), std::move(resource_urls));
}

ScriptWebBundleRule::ScriptWebBundleRule(
    const KURL& source_url,
    network::mojom::CredentialsMode credentials_mode,
    HashSet<KURL> scope_urls,
    HashSet<KURL> resource_urls)
    : source_url_(source_url),
      credentials_mode_(credentials_mode),
      scope_urls_(std::move(scope_urls)),
      resource_urls_(std::move(resource_urls)) {}

bool ScriptWebBundleRule::ResourcesOrScopesMatch(const KURL& url) const {
  if (resource_urls_.Contains(url))
    return true;
  for (const auto& scope : scope_urls_) {
    if (url.GetString().StartsWith(scope.GetString()))
      return true;
  }
  return false;
}

}  // namespace blink
