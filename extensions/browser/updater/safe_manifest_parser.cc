// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/safe_manifest_parser.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "base/version.h"
#include "content/public/browser/browser_thread.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/data_decoder/public/cpp/safe_xml_parser.h"

namespace extensions {

using data_decoder::GetXmlElementAttribute;
using data_decoder::GetXmlElementChildWithTag;
using data_decoder::GetXmlElementNamespacePrefix;
using data_decoder::GetXmlQualifiedName;
using data_decoder::IsXmlElementNamed;

namespace {

constexpr char kExpectedGupdateProtocol[] = "2.0";
constexpr char kExpectedGupdateXmlns[] =
    "http://www.google.com/update2/response";

// Helper function that reads in values for a single <app> tag. It returns a
// boolean indicating success or failure. On failure, it writes a error message
// into |result->parse_error|.
bool ParseSingleAppTag(const base::Value& app_element,
                       const std::string& xml_namespace,
                       UpdateManifestResult* result,
                       int* prodversionmin_count) {
  // Read the extension id.
  result->extension_id = GetXmlElementAttribute(app_element, "appid");
  if (result->extension_id.empty()) {
    result->parse_error =
        ManifestParseFailure(std::move("Missing appid on app node"),
                             ManifestInvalidError::MISSING_APP_ID);
    return false;
  }

  // Get the app status.
  result->app_status = GetXmlElementAttribute(app_element, "status");
  if (!result->app_status.empty() && result->app_status != "ok") {
    result->parse_error = ManifestParseFailure(
        "App status is not OK", ManifestInvalidError::BAD_APP_STATUS);
    return false;
  }

  // Get the updatecheck node.
  std::string updatecheck_name =
      GetXmlQualifiedName(xml_namespace, "updatecheck");
  int updatecheck_count =
      data_decoder::GetXmlElementChildrenCount(app_element, updatecheck_name);
  if (updatecheck_count != 1) {
    result->parse_error = ManifestParseFailure(
        updatecheck_count == 0
            ? std::move("Missing updatecheck on app.")
            : std::move("Too many updatecheck tags on app (expecting only 1)."),
        updatecheck_count == 0
            ? ManifestInvalidError::MISSING_UPDATE_CHECK_TAGS
            : ManifestInvalidError::MULTIPLE_UPDATE_CHECK_TAGS);
    return false;
  }

  const base::Value* updatecheck =
      data_decoder::GetXmlElementChildWithTag(app_element, updatecheck_name);

  result->status = GetXmlElementAttribute(*updatecheck, "status");
  if (result->status == "noupdate") {
    result->info = GetXmlElementAttribute(*updatecheck, "info");
    return true;
  }

  // Get the optional minimum browser version.
  result->browser_min_version =
      GetXmlElementAttribute(*updatecheck, "prodversionmin");
  if (!result->browser_min_version.empty()) {
    *prodversionmin_count += 1;
    base::Version browser_min_version(result->browser_min_version);
    if (!browser_min_version.IsValid()) {
      std::string error_detail;
      error_detail = "Invalid prodversionmin: '";
      error_detail += result->browser_min_version;
      error_detail += "'.";
      result->parse_error =
          ManifestParseFailure(std::move(error_detail),
                               ManifestInvalidError::INVALID_PRODVERSION_MIN);
      return false;
    }
  }

  // Find the url to the crx file.
  result->crx_url = GURL(GetXmlElementAttribute(*updatecheck, "codebase"));

  if (!result->crx_url.is_valid()) {
    if (result->crx_url.is_empty()) {
      result->parse_error =
          ManifestParseFailure(std::move("Empty codebase url."),
                               ManifestInvalidError::EMPTY_CODEBASE_URL);
      return false;
    }
    std::string error_detail;
    error_detail = "Invalid codebase url: '";
    error_detail += result->crx_url.possibly_invalid_spec();
    error_detail += "'.";
    result->parse_error = ManifestParseFailure(
        std::move(error_detail), ManifestInvalidError::INVALID_CODEBASE_URL);
    return false;
  }

  // Get the version.
  result->version = GetXmlElementAttribute(*updatecheck, "version");
  if (result->version.empty()) {
    result->parse_error = ManifestParseFailure(
        std::move("Missing version for updatecheck."),
        ManifestInvalidError::MISSING_VERSION_FOR_UPDATE_CHECK);
    return false;
  }
  base::Version version(result->version);
  if (!version.IsValid()) {
    std::string error_detail;
    error_detail = "Invalid version: '";
    error_detail += result->version;
    error_detail += "'.";
    result->parse_error = ManifestParseFailure(
        std::move(error_detail), ManifestInvalidError::INVALID_VERSION);
    return false;
  }

  // package_hash is optional. It is a sha256 hash of the package in hex format.
  result->package_hash = GetXmlElementAttribute(*updatecheck, "hash_sha256");

  int size = 0;
  if (base::StringToInt(GetXmlElementAttribute(*updatecheck, "size"), &size)) {
    result->size = size;
  }

  // package_fingerprint is optional. It identifies the package, preferably
  // with a modified sha256 hash of the package in hex format.
  result->package_fingerprint = GetXmlElementAttribute(*updatecheck, "fp");

  // Differential update information is optional.
  result->diff_crx_url =
      GURL(GetXmlElementAttribute(*updatecheck, "codebasediff"));
  result->diff_package_hash = GetXmlElementAttribute(*updatecheck, "hashdiff");
  int sizediff = 0;
  if (base::StringToInt(GetXmlElementAttribute(*updatecheck, "sizediff"),
                        &sizediff)) {
    result->diff_size = sizediff;
  }

  return true;
}

void ParseXmlDone(ParseUpdateManifestCallback callback,
                  data_decoder::DataDecoder::ValueOrError result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::string gupdate_ns;
  const auto get_root =
      [&]() -> base::expected<base::Value, ManifestParseFailure> {
    ASSIGN_OR_RETURN(base::Value root, std::move(result),
                     [](std::string error) {
                       return ManifestParseFailure(
                           "Failed to parse XML: " + std::move(error),
                           ManifestInvalidError::XML_PARSING_FAILED);
                     });

    // Look for the required namespace declaration.
    if (!GetXmlElementNamespacePrefix(root, kExpectedGupdateXmlns,
                                      &gupdate_ns)) {
      return base::unexpected(ManifestParseFailure(
          "Missing or incorrect xmlns on gupdate tag",
          ManifestInvalidError::INVALID_XLMNS_ON_GUPDATE_TAG));
    }

    if (!IsXmlElementNamed(root, GetXmlQualifiedName(gupdate_ns, "gupdate"))) {
      return base::unexpected(ManifestParseFailure(
          "Missing gupdate tag", ManifestInvalidError::MISSING_GUPDATE_TAG));
    }

    // Check for the gupdate "protocol" attribute.
    if (GetXmlElementAttribute(root, "protocol") != kExpectedGupdateProtocol) {
      return base::unexpected(ManifestParseFailure(
          std::string("Missing/incorrect protocol on gupdate tag (expected '") +
              kExpectedGupdateProtocol + "')",
          ManifestInvalidError::INVALID_PROTOCOL_ON_GUPDATE_TAG));
    }

    return root;
  };
  ASSIGN_OR_RETURN(
      base::Value root, get_root(), [&](ManifestParseFailure error) {
        std::move(callback).Run(/*results=*/nullptr, std::move(error));
      });

  auto results = std::make_unique<UpdateManifestResults>();

  // Parse the first <daystart> if it's present.
  const base::Value* daystart = GetXmlElementChildWithTag(
      root, GetXmlQualifiedName(gupdate_ns, "daystart"));
  if (daystart) {
    std::string elapsed_seconds =
        GetXmlElementAttribute(*daystart, "elapsed_seconds");
    int parsed_elapsed = kNoDaystart;
    if (base::StringToInt(elapsed_seconds, &parsed_elapsed)) {
      results->daystart_elapsed_seconds = parsed_elapsed;
    }
  }

  // Parse each of the <app> tags.
  std::vector<const base::Value*> apps;
  data_decoder::GetAllXmlElementChildrenWithTag(
      root, GetXmlQualifiedName(gupdate_ns, "app"), &apps);
  std::string error_msg;
  int prodversionmin_count = 0;
  for (const auto* app : apps) {
    UpdateManifestResult manifest_result;
    ParseSingleAppTag(*app, gupdate_ns, &manifest_result,
                      &prodversionmin_count);
    results->update_list.push_back(manifest_result);
  }
  // Parsing error corresponding to each extension are stored in the results.
  std::move(callback).Run(std::move(results), std::nullopt);
}

}  // namespace

ManifestParseFailure::ManifestParseFailure() = default;

ManifestParseFailure::ManifestParseFailure(const ManifestParseFailure& other) =
    default;

ManifestParseFailure::ManifestParseFailure(std::string error_detail,
                                           ManifestInvalidError error)
    : error_detail(std::move(error_detail)), error(error) {}

ManifestParseFailure::~ManifestParseFailure() = default;

UpdateManifestResult::UpdateManifestResult() = default;

UpdateManifestResult::UpdateManifestResult(const UpdateManifestResult& other) =
    default;

UpdateManifestResult::~UpdateManifestResult() = default;

UpdateManifestResults::UpdateManifestResults() = default;

UpdateManifestResults::UpdateManifestResults(
    const UpdateManifestResults& other) = default;

UpdateManifestResults::~UpdateManifestResults() = default;

std::map<std::string, std::vector<const UpdateManifestResult*>>
UpdateManifestResults::GroupSuccessfulByID() const {
  std::map<std::string, std::vector<const UpdateManifestResult*>> groups;
  for (const UpdateManifestResult& update_result : update_list) {
    if (!update_result.parse_error) {
      groups[update_result.extension_id].push_back(&update_result);
    }
  }
  return groups;
}

void ParseUpdateManifest(const std::string& xml,
                         ParseUpdateManifestCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(callback);
  data_decoder::DataDecoder::ParseXmlIsolated(
      xml, data_decoder::mojom::XmlParser::WhitespaceBehavior::kIgnore,
      base::BindOnce(&ParseXmlDone, std::move(callback)));
}

}  // namespace extensions
