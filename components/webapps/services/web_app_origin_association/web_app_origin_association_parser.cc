// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/services/web_app_origin_association/web_app_origin_association_parser.h"

#include <string>

#include "base/json/json_reader.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/webapps/services/web_app_origin_association/web_app_origin_association_uma_util.h"
#include "url/gurl.h"

namespace {

constexpr char kWebAppsKey[] = "web_apps";
constexpr char kWebAppIdentity[] = "web_app_identity";

}  // anonymous namespace

namespace webapps {

WebAppOriginAssociationParser::WebAppOriginAssociationParser() = default;

WebAppOriginAssociationParser::~WebAppOriginAssociationParser() = default;

mojom::WebAppOriginAssociationPtr WebAppOriginAssociationParser::Parse(
    const std::string& data) {
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
    association->apps = ParseAssociatedWebApps(parsed_data.GetDict());
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
    const base::Value::Dict& root_dict) {
  std::vector<mojom::AssociatedWebAppPtr> result;
  const base::Value::List* apps_value = root_dict.FindList(kWebAppsKey);
  if (!apps_value) {
    if (root_dict.contains(kWebAppsKey)) {
      AddErrorInfo("Property '" + std::string(kWebAppsKey) +
                   "' ignored, type array expected.");
      return result;
    }

    AddErrorInfo("Origin association ignored. Required property '" +
                 std::string(kWebAppsKey) + "' expected.");
    return result;
  }

  for (const auto& app_item : *apps_value) {
    if (!app_item.is_dict()) {
      AddErrorInfo("Associated app ignored, type object expected.");
      continue;
    }

    std::optional<mojom::AssociatedWebAppPtr> app =
        ParseAssociatedWebApp(app_item.GetDict());
    if (!app)
      continue;

    result.push_back(std::move(app.value()));
  }

  return result;
}

std::optional<mojom::AssociatedWebAppPtr>
WebAppOriginAssociationParser::ParseAssociatedWebApp(
    const base::Value::Dict& app_dict) {
  const std::string* web_app_identity_url_value =
      app_dict.FindString(kWebAppIdentity);
  if (!web_app_identity_url_value) {
    if (app_dict.contains(kWebAppIdentity)) {
      AddErrorInfo("Associated app ignored. Required property '" +
                   std::string(kWebAppIdentity) + "' is not a string.");
      return std::nullopt;
    }

    AddErrorInfo("Associated app ignored. Required property '" +
                 std::string(kWebAppIdentity) + "' does not exist.");
    return std::nullopt;
  }

  GURL web_app_identity(*web_app_identity_url_value);
  if (!web_app_identity.is_valid()) {
    AddErrorInfo("Associated app ignored. Required property '" +
                 std::string(kWebAppIdentity) + "' is not a valid URL.");
    return std::nullopt;
  }

  mojom::AssociatedWebAppPtr app = mojom::AssociatedWebApp::New();
  app->web_app_identity = std::move(web_app_identity);
  return app;
}

void WebAppOriginAssociationParser::AddErrorInfo(const std::string& error_msg,
                                                 int error_line,
                                                 int error_column) {
  mojom::WebAppOriginAssociationErrorPtr error =
      mojom::WebAppOriginAssociationError::New(error_msg, error_line,
                                               error_column);
  errors_.push_back(std::move(error));
}

}  // namespace webapps
