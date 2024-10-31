// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/services/web_app_origin_association/web_app_origin_association_parser.h"

#include <string>

#include "base/json/json_reader.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/webapps/services/web_app_origin_association/web_app_origin_association_uma_util.h"
#include "mojo/public/cpp/bindings/lib/string_serialization.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

constexpr char kExtendedScope[] = "scope";

}  // anonymous namespace

namespace webapps {

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

WebAppOriginAssociationParser::WebAppOriginAssociationParser() = default;

WebAppOriginAssociationParser::~WebAppOriginAssociationParser() = default;

mojom::WebAppOriginAssociationPtr WebAppOriginAssociationParser::Parse(
    const std::string& data,
    const url::Origin& origin) {
  using Result = webapps::WebAppOriginAssociationMetrics::ParseResult;
  auto result =
      [&]() -> base::expected<mojom::WebAppOriginAssociationPtr, Result> {
    ASSIGN_OR_RETURN(auto parsed_data,
                     base::JSONReader::ReadAndReturnValueWithError(data),
                     [&](base::JSONReader::Error error) {
                       AddErrorInfo(error.message, error.line, error.column);
                       return Result::kParseFailedInvalidJson;
                     });
    if (!parsed_data.is_dict()) {
      AddErrorInfo("No valid JSON object found.");
      return base::unexpected(Result::kParseFailedNotADictionary);
    }

    auto association = mojom::WebAppOriginAssociation::New();
    association->apps = ParseAssociatedWebApps(parsed_data.GetDict(), origin);
    return association;
  }();
  webapps::WebAppOriginAssociationMetrics::RecordParseResult(
      result.error_or(Result::kParseSucceeded));
  failed_ |= !result.has_value();
  return std::move(result).value_or(nullptr);
}

bool WebAppOriginAssociationParser::failed() const {
  return failed_;
}

std::vector<mojom::WebAppOriginAssociationErrorPtr>
WebAppOriginAssociationParser::GetErrors() {
  auto result = std::move(errors_);
  errors_.clear();
  return result;
}

std::vector<mojom::AssociatedWebAppPtr>
WebAppOriginAssociationParser::ParseAssociatedWebApps(
    const base::Value::Dict& root_dict,
    const url::Origin& origin) {
  std::vector<mojom::AssociatedWebAppPtr> result;
  if (root_dict.empty()) {
    AddErrorInfo(kWebAppOriginAssociationParserFormatError);
    return result;
  }
  for (const auto iter : root_dict) {
    GURL web_app_manifest_id(iter.first);
    if (!web_app_manifest_id.is_valid()) {
      AddErrorInfo(kInvalidManifestId);
      continue;
    }

    if (!iter.second.is_dict()) {
      AddErrorInfo(kInvalidValueType);
      continue;
    }

    std::optional<GURL> extended_scope =
        ParseExtendedScope(iter.second.GetDict(), origin);
    if (!extended_scope) {
      continue;
    }

    mojom::AssociatedWebAppPtr app = mojom::AssociatedWebApp::New();
    app->web_app_identity = std::move(web_app_manifest_id);
    app->scope = std::move(extended_scope.value());
    result.push_back(std::move(app));
  }

  return result;
}

std::optional<GURL> WebAppOriginAssociationParser::ParseExtendedScope(
    const base::Value::Dict& extended_scope_info,
    const url::Origin& origin) {
  const std::string* extended_scope_ptr =
      extended_scope_info.FindString(kExtendedScope);
  if (!extended_scope_ptr || extended_scope_ptr->empty()) {
    // No explicit `scope` defaults to root ie the scope of associate's origin.
    return origin.GetURL();
  }
  GURL result = origin.GetURL().Resolve(*extended_scope_ptr);
  if (!result.is_valid()) {
    AddErrorInfo(kInvalidScopeUrl);
    return std::nullopt;
  }
  if (!UrlIsWithinScope(result, origin)) {
    AddErrorInfo(kInvalidScopeUrl);
    return std::nullopt;
  }
  return result;
}

void WebAppOriginAssociationParser::AddErrorInfo(const std::string& error_msg,
                                                 int error_line,
                                                 int error_column) {
  mojom::WebAppOriginAssociationErrorPtr error =
      mojom::WebAppOriginAssociationError::New(error_msg, error_line,
                                               error_column);
  errors_.push_back(std::move(error));
}

// Determines whether |url| is within scope of |scope|.
bool WebAppOriginAssociationParser::UrlIsWithinScope(
    const GURL& url,
    const url::Origin& extended_origin) {
  // Uses the same within-scope rules as WebAppRegistrar::IsUrlInAppScope
  return base::StartsWith(url.spec(), extended_origin.GetURL().spec(),
                          base::CompareCase::SENSITIVE) > 0;
}

}  // namespace webapps
