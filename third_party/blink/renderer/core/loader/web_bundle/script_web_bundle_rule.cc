// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/web_bundle/script_web_bundle_rule.h"

#include "third_party/blink/renderer/platform/json/json_parser.h"
#include "third_party/blink/renderer/platform/json/json_values.h"

namespace blink {

namespace {

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

absl::optional<ScriptWebBundleRule> ScriptWebBundleRule::ParseJson(
    const String& inline_text,
    const KURL& base_url) {
  // TODO(crbug.com/1245166): Emit a user friendly parse error message.

  std::unique_ptr<JSONValue> json = ParseJSON(inline_text);
  if (!json)
    return absl::nullopt;
  std::unique_ptr<JSONObject> json_obj = JSONObject::From(std::move(json));
  if (!json_obj)
    return absl::nullopt;

  String source;
  if (!json_obj->GetString("source", &source))
    return absl::nullopt;
  KURL source_url(base_url, source);
  if (!source_url.IsValid())
    return absl::nullopt;

  network::mojom::CredentialsMode credentials_mode;
  String credentials;
  if (json_obj->GetString("credentials", &credentials)) {
    credentials_mode = ParseCredentials(credentials);
  } else {
    // The default is "same-origin".
    credentials_mode = network::mojom::CredentialsMode::kSameOrigin;
  }

  HashSet<KURL> scope_urls =
      ParseJSONArrayAsURLs(json_obj->GetArray("scopes"), source_url);

  HashSet<KURL> resource_urls =
      ParseJSONArrayAsURLs(json_obj->GetArray("resources"), source_url);

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
