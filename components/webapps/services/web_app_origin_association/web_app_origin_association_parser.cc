// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/services/web_app_origin_association/web_app_origin_association_parser.h"

#include <optional>
#include <string>

#include "base/json/json_reader.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/webapps/services/web_app_origin_association/web_app_origin_association_uma_util.h"
#include "mojo/public/cpp/bindings/lib/string_serialization.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace webapps {

namespace {

constexpr char kExtendedScope[] = "scope";
constexpr char kAllowMigration[] = "allow_migration";

// Determines whether |url| is within scope of |extended_origin|'s path.
bool UrlIsWithinScope(const GURL& url, const url::Origin& extended_origin) {
  return extended_origin.IsSameOriginWith(url) &&
         url.GetPath().starts_with(extended_origin.GetURL().GetPath());
}

std::optional<GURL> ParseExtendedScope(
    const base::DictValue& extended_scope_info,
    const url::Origin& associate_origin) {
  const std::string* extended_scope_ptr =
      extended_scope_info.FindString(kExtendedScope);
  if (!extended_scope_ptr || extended_scope_ptr->empty()) {
    // No explicit `scope` defaults to root ie the scope of associate's origin.
    return associate_origin.GetURL();
  }
  GURL associate_extended_url =
      associate_origin.GetURL().Resolve(*extended_scope_ptr);
  if (!associate_extended_url.is_valid()) {
    return std::nullopt;
  }
  if (!UrlIsWithinScope(associate_extended_url, associate_origin)) {
    return std::nullopt;
  }
  return associate_extended_url;
}

ParsedAssociations ParseAssociatedWebApps(const base::DictValue& root_dict,
                                          const url::Origin& origin) {
  ParsedAssociations result;
  if (root_dict.empty()) {
    result.warnings.push_back(kWebAppOriginAssociationParserFormatError);
    return result;
  }
  for (const auto iter : root_dict) {
    GURL web_app_manifest_id(iter.first);
    if (!web_app_manifest_id.is_valid()) {
      result.warnings.push_back(kInvalidManifestId);
      continue;
    }

    if (!iter.second.is_dict()) {
      result.warnings.push_back(kInvalidValueType);
      continue;
    }

    std::optional<GURL> extended_scope =
        ParseExtendedScope(iter.second.GetDict(), origin);
    GURL scope_url;
    if (extended_scope) {
      scope_url = std::move(extended_scope).value();
    } else {
      result.warnings.push_back(kInvalidScopeUrl);
    }

    bool allow_migration =
        iter.second.GetDict().FindBool(kAllowMigration).value_or(false);

    result.apps.push_back({.web_app_identity = std::move(web_app_manifest_id),
                           .scope = std::move(scope_url),
                           .allow_migration = allow_migration});
  }

  return result;
}

}  // namespace

const char kWebAppOriginAssociationParserFormatError[] =
    R"("Invalid association format. Associations must start with a valid
    manifest id e.g.
    {
     "https://example.com/app" : {
       "scope": "/"
      }
    })";
const char kInvalidManifestId[] =
    "Associated app ignored. Manifest ID is not a valid URL.";
// Value refers to the key:value pair. The value must be a dictionary/JSON
// object.
const char kInvalidValueType[] =
    "Associated app ignored, type object expected.";
const char kInvalidScopeUrl[] =
    "Associated app ignored. Required property 'scope' is not a valid URL.";

ParsedAssociations::ParsedAssociations() = default;
ParsedAssociations::~ParsedAssociations() = default;

ParsedAssociations::ParsedAssociations(const ParsedAssociations&) = default;
ParsedAssociations& ParsedAssociations::operator=(const ParsedAssociations&) =
    default;

ParsedAssociations::ParsedAssociations(ParsedAssociations&&) = default;
ParsedAssociations& ParsedAssociations::operator=(ParsedAssociations&&) =
    default;

base::expected<ParsedAssociations, std::string> ParseWebAppOriginAssociations(
    const std::string& data,
    const url::Origin& origin) {
  using ParseResult = webapps::WebAppOriginAssociationMetrics::ParseResult;

  auto json_result = base::JSONReader::ReadAndReturnValueWithError(
      data, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!json_result.has_value()) {
    webapps::WebAppOriginAssociationMetrics::RecordParseResult(
        ParseResult::kParseFailedInvalidJson);
    return base::unexpected(json_result.error().ToString());
  }
  const auto* dict = json_result->GetIfDict();
  if (!dict) {
    webapps::WebAppOriginAssociationMetrics::RecordParseResult(
        ParseResult::kParseFailedNotADictionary);
    return base::unexpected("No valid JSON object found.");
  }
  webapps::WebAppOriginAssociationMetrics::RecordParseResult(
      ParseResult::kParseSucceeded);

  return ParseAssociatedWebApps(*dict, origin);
}

}  // namespace webapps
