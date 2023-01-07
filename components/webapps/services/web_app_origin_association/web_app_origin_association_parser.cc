// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/services/web_app_origin_association/web_app_origin_association_parser.h"

#include "base/json/json_reader.h"
#include "base/values.h"
#include "components/webapps/services/web_app_origin_association/web_app_origin_association_uma_util.h"
#include "url/gurl.h"

namespace {

constexpr char kWebAppsKey[] = "web_apps";
constexpr char kManifestUrlKey[] = "manifest";
constexpr char kAppDetailsKey[] = "details";
constexpr char kPathsKey[] = "paths";
constexpr char kExcludePathsKey[] = "exclude_paths";

}  // anonymous namespace

namespace webapps {

WebAppOriginAssociationParser::WebAppOriginAssociationParser() = default;

WebAppOriginAssociationParser::~WebAppOriginAssociationParser() = default;

mojom::WebAppOriginAssociationPtr WebAppOriginAssociationParser::Parse(
    const std::string& data) {
  auto parsed_data = base::JSONReader::ReadAndReturnValueWithError(data);

  if (!parsed_data.has_value()) {
    AddErrorInfo(parsed_data.error().message, parsed_data.error().line,
                 parsed_data.error().column);
    failed_ = true;
    webapps::WebAppOriginAssociationMetrics::RecordParseResult(
        webapps::WebAppOriginAssociationMetrics::ParseResult::
            kParseFailedInvalidJson);
    return nullptr;
  }
  if (!parsed_data->is_dict()) {
    AddErrorInfo("No valid JSON object found.");
    failed_ = true;
    webapps::WebAppOriginAssociationMetrics::RecordParseResult(
        webapps::WebAppOriginAssociationMetrics::ParseResult::
            kParseFailedNotADictionary);
    return nullptr;
  }

  mojom::WebAppOriginAssociationPtr association =
      mojom::WebAppOriginAssociation::New();
  association->apps = ParseAssociatedWebApps(*parsed_data);
  webapps::WebAppOriginAssociationMetrics::RecordParseResult(
      webapps::WebAppOriginAssociationMetrics::ParseResult::kParseSucceeded);
  return association;
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
    const base::Value& root_dict) {
  std::vector<mojom::AssociatedWebAppPtr> result;
  const base::Value* apps_value = root_dict.FindKey(kWebAppsKey);
  if (!apps_value) {
    AddErrorInfo("Origin association ignored. Required property '" +
                 std::string(kWebAppsKey) + "' expected.");
    return result;
  }

  if (!apps_value->is_list()) {
    AddErrorInfo("Property '" + std::string(kWebAppsKey) +
                 "' ignored, type array expected.");
    return result;
  }

  for (const auto& app_item : apps_value->GetList()) {
    if (!app_item.is_dict()) {
      AddErrorInfo("Associated app ignored, type object expected.");
      continue;
    }

    absl::optional<mojom::AssociatedWebAppPtr> app =
        ParseAssociatedWebApp(app_item);
    if (!app)
      continue;

    result.push_back(std::move(app.value()));
  }

  return result;
}

absl::optional<mojom::AssociatedWebAppPtr>
WebAppOriginAssociationParser::ParseAssociatedWebApp(
    const base::Value& app_dict) {
  absl::optional<GURL> manifest_url = ParseManifestURL(app_dict);
  if (!manifest_url)
    return absl::nullopt;

  mojom::AssociatedWebAppPtr app = mojom::AssociatedWebApp::New();
  app->manifest_url = manifest_url.value();

  const base::Value* app_details_value = app_dict.FindKey(kAppDetailsKey);
  if (!app_details_value)
    return app;

  if (!app_details_value->is_dict()) {
    AddErrorInfo("Property '" + std::string(kAppDetailsKey) +
                 "' ignored, type dictionary expected.");
    return app;
  }

  absl::optional<std::vector<std::string>> paths =
      ParsePaths(*app_details_value, kPathsKey);
  if (paths)
    app->paths = paths.value();
  absl::optional<std::vector<std::string>> exclude_paths =
      ParsePaths(*app_details_value, kExcludePathsKey);
  if (exclude_paths)
    app->exclude_paths = exclude_paths.value();
  return app;
}

absl::optional<GURL> WebAppOriginAssociationParser::ParseManifestURL(
    const base::Value& app_dict) {
  const base::Value* url_value = app_dict.FindKey(kManifestUrlKey);
  if (!url_value) {
    AddErrorInfo("Associated app ignored. Required property '" +
                 std::string(kManifestUrlKey) + "' does not exist.");
    return absl::nullopt;
  }

  if (!url_value->is_string()) {
    AddErrorInfo("Associated app ignored. Required property '" +
                 std::string(kManifestUrlKey) + "' is not a string.");
    return absl::nullopt;
  }

  GURL manifest_url(url_value->GetString());
  if (!manifest_url.is_valid()) {
    AddErrorInfo("Associated app ignored. Required property '" +
                 std::string(kManifestUrlKey) + "' is not a valid URL.");
    return absl::nullopt;
  }

  return manifest_url;
}

absl::optional<std::vector<std::string>>
WebAppOriginAssociationParser::ParsePaths(const base::Value& app_details_dict,
                                          const std::string& key) {
  const base::Value* paths_value = app_details_dict.FindKey(key);
  if (!paths_value)
    return absl::nullopt;

  if (!paths_value->is_list()) {
    AddErrorInfo("Property '" + key + "' ignored, type array expected.");
    return absl::nullopt;
  }

  std::vector<std::string> paths;
  for (const auto& path_item : paths_value->GetList()) {
    if (!path_item.is_string()) {
      AddErrorInfo(key + " entry ignored, type string expected.");
      continue;
    }

    paths.push_back(path_item.GetString());
  }

  return paths;
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
