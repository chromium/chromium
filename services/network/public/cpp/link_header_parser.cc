// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/link_header_parser.h"

#include <algorithm>
#include <string>
#include <unordered_map>

#include "base/strings/string_util.h"
#include "components/link_header_util/link_header_util.h"
#include "net/base/mime_util.h"
#include "net/http/http_response_headers.h"

namespace network {

namespace {

bool IsValidMimeType(const std::string& type_string) {
  std::string top_level_type;
  if (!net::ParseMimeTypeWithoutParameter(type_string, &top_level_type,
                                          /*subtype=*/nullptr)) {
    return false;
  }

  return net::IsValidTopLevelMimeType(top_level_type);
}

// Parses `rel` attribute and returns its parsed representation. Returns
// std::nullopt when the value isn't pre-defined.
std::optional<mojom::LinkRelAttribute> ParseRelAttribute(
    const std::optional<std::string>& attr) {
  if (!attr.has_value())
    return std::nullopt;

  std::string value = base::ToLowerASCII(attr.value());
  if (value == "dns-prefetch")
    return mojom::LinkRelAttribute::kDnsPrefetch;
  if (value == "preconnect")
    return mojom::LinkRelAttribute::kPreconnect;
  if (value == "preload")
    return mojom::LinkRelAttribute::kPreload;
  else if (value == "modulepreload")
    return mojom::LinkRelAttribute::kModulePreload;
  return std::nullopt;
}

// Parses `as` attribute and returns its parsed representation. Returns
// std::nullopt when the value isn't pre-defined.
std::optional<mojom::LinkAsAttribute> ParseAsAttribute(
    const std::optional<std::string>& attr) {
  if (!attr.has_value())
    return std::nullopt;

  std::string value = base::ToLowerASCII(attr.value());
  if (value == "font") {
    return mojom::LinkAsAttribute::kFont;
  } else if (value == "image") {
    return mojom::LinkAsAttribute::kImage;
  } else if (value == "script") {
    return mojom::LinkAsAttribute::kScript;
  } else if (value == "style") {
    return mojom::LinkAsAttribute::kStyleSheet;
  } else if (value == "fetch") {
    return mojom::LinkAsAttribute::kFetch;
  }
  return std::nullopt;
}

// Parses `crossorigin` attribute and returns its parsed representation. Returns
// std::nullopt when the value isn't pre-defined.
std::optional<mojom::CrossOriginAttribute> ParseCrossOriginAttribute(
    const std::optional<std::string>& attr) {
  if (!attr.has_value())
    return mojom::CrossOriginAttribute::kAnonymous;

  std::string value = base::ToLowerASCII(attr.value());
  if (value == "anonymous")
    return mojom::CrossOriginAttribute::kAnonymous;
  else if (value == "use-credentials")
    return mojom::CrossOriginAttribute::kUseCredentials;
  return std::nullopt;
}

// Parses `fetchpriority` attribute and returns its parsed representation.
// Returns mojom::FetchPriorityAttribute::kAuto which is the missing and
// invalid value for the attribute.
std::optional<mojom::FetchPriorityAttribute> ParseFetchPriorityAttribute(
    const std::optional<std::string>& attr) {
  if (!attr.has_value()) {
    return mojom::FetchPriorityAttribute::kAuto;
  }

  std::string value = base::ToLowerASCII(attr.value());
  if (value == "low") {
    return mojom::FetchPriorityAttribute::kLow;
  } else if (value == "high") {
    return mojom::FetchPriorityAttribute::kHigh;
  } else if (value == "auto") {
    return mojom::FetchPriorityAttribute::kAuto;
  }
  return mojom::FetchPriorityAttribute::kAuto;
}

// Parses attributes of a Link header and populates parsed representations of
// attributes. Returns true only when all attributes and their values are
// pre-definied.
bool ParseAttributes(
    const std::unordered_map<std::string, std::optional<std::string>>& attrs,
    mojom::LinkHeaderPtr& parsed) {
  bool is_rel_set = false;

  for (const auto& attr : attrs) {
    std::string name = base::ToLowerASCII(attr.first);

    if (name == "rel") {
      // Ignore if `rel` is already set.
      if (is_rel_set)
        continue;
      std::optional<mojom::LinkRelAttribute> rel =
          ParseRelAttribute(attr.second);
      if (!rel.has_value())
        return false;
      parsed->rel = rel.value();
      is_rel_set = true;
    } else if (name == "as") {
      // TODO(crbug.com/40170852): Make sure ignoring second and subsequent ones
      // is a reasonable behavior.
      if (parsed->as != mojom::LinkAsAttribute::kUnspecified)
        continue;
      std::optional<mojom::LinkAsAttribute> as = ParseAsAttribute(attr.second);
      if (!as.has_value())
        return false;
      parsed->as = as.value();
    } else if (name == "crossorigin") {
      // TODO(crbug.com/40170852): Make sure ignoring second and subsequent ones
      // is a reasonable behavior.
      if (parsed->cross_origin != mojom::CrossOriginAttribute::kUnspecified)
        continue;
      std::optional<mojom::CrossOriginAttribute> cross_origin =
          ParseCrossOriginAttribute(attr.second);
      if (!cross_origin.has_value())
        return false;
      parsed->cross_origin = cross_origin.value();
    } else if (name == "type") {
      // TODO(crbug.com/40170852): Make sure ignoring second and subsequent ones
      // is a reasonable behavior.
      if (parsed->mime_type.has_value())
        continue;
      if (!attr.second.has_value() || !IsValidMimeType(attr.second.value()))
        return false;
      parsed->mime_type = attr.second.value();
    } else if (name == "fetchpriority") {
      // TODO(crbug.com/40170852): Make sure ignoring second and subsequent ones
      // is a reasonable behavior.
      if (parsed->fetch_priority != mojom::FetchPriorityAttribute::kAuto) {
        continue;
      }
      std::optional<mojom::FetchPriorityAttribute> fetch_priority =
          ParseFetchPriorityAttribute(attr.second);
      if (!fetch_priority.has_value()) {
        return false;
      }
      parsed->fetch_priority = fetch_priority.value();
    } else {
      // The current Link header contains an attribute which isn't pre-defined.
      return false;
    }
  }

  // `rel` must be present.
  return is_rel_set;
}

}  // namespace

std::vector<mojom::LinkHeaderPtr> ParseLinkHeaders(
    const net::HttpResponseHeaders& headers,
    const GURL& base_url) {
  std::vector<mojom::LinkHeaderPtr> parsed_headers;
  std::string link_header;
  headers.GetNormalizedHeader("link", &link_header);
  for (const auto& pair : link_header_util::SplitLinkHeader(link_header)) {
    std::string url;
    std::unordered_map<std::string, std::optional<std::string>> attrs;
    if (!link_header_util::ParseLinkHeaderValue(pair.first, pair.second, &url,
                                                &attrs)) {
      continue;
    }

    auto parsed = mojom::LinkHeader::New();

    parsed->href = base_url.Resolve(url);
    if (!parsed->href.is_valid())
      continue;

    if (!ParseAttributes(attrs, parsed))
      continue;

    parsed_headers.push_back(std::move(parsed));
  }

  return parsed_headers;
}

}  // namespace network
