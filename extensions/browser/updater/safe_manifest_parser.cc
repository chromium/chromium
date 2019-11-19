// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/safe_manifest_parser.h"

#include <memory>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
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

void ReportError(ParseUpdateManifestCallback callback,
                 const std::string& error) {
  std::move(callback).Run(/*results=*/nullptr, error);
}

// Helper function that reads in values for a single <app> tag. It returns a
// boolean indicating success or failure. On failure, it writes a error message
// into |error_detail|.
bool ParseSingleAppTag(const base::Value& app_element,
                       const std::string& xml_namespace,
                       UpdateManifestResult* result,
                       std::string* error_detail,
                       int* prodversionmin_count) {
  // Read the extension id.
  result->extension_id = GetXmlElementAttribute(app_element, "appid");
  if (result->extension_id.empty()) {
    *error_detail = "Missing appid on app node";
    return false;
  }

  // Get the updatecheck node.
  std::string updatecheck_name =
      GetXmlQualifiedName(xml_namespace, "updatecheck");
  int updatecheck_count =
      data_decoder::GetXmlElementChildrenCount(app_element, updatecheck_name);
  if (updatecheck_count != 1) {
    *error_detail = updatecheck_count == 0
                        ? "Too many updatecheck tags on app (expecting only 1)."
                        : "Missing updatecheck on app.";
    return false;
  }

  const base::Value* updatecheck =
      data_decoder::GetXmlElementChildWithTag(app_element, updatecheck_name);

  if (GetXmlElementAttribute(*updatecheck, "status") == "noupdate") {
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
      *error_detail = "Invalid prodversionmin: '";
      *error_detail += result->browser_min_version;
      *error_detail += "'.";
      return false;
    }
  }

  // Find the url to the crx file.
  result->crx_url = GURL(GetXmlElementAttribute(*updatecheck, "codebase"));
  if (!result->crx_url.is_valid()) {
    *error_detail = "Invalid codebase url: '";
    *error_detail += result->crx_url.possibly_invalid_spec();
    *error_detail += "'.";
    return false;
  }

  // Get the version.
  result->version = GetXmlElementAttribute(*updatecheck, "version");
  if (result->version.empty()) {
    *error_detail = "Missing version for updatecheck.";
    return false;
  }
  base::Version version(result->version);
  if (!version.IsValid()) {
    *error_detail = "Invalid version: '";
    *error_detail += result->version;
    *error_detail += "'.";
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

  if (!result.value) {
    ReportError(std::move(callback), "Failed to parse XML: " + *result.error);
    return;
  }

  auto results = std::make_unique<UpdateManifestResults>();
  base::Value& root = *result.value;

  // Look for the required namespace declaration.
  std::string gupdate_ns;
  if (!GetXmlElementNamespacePrefix(root, kExpectedGupdateXmlns, &gupdate_ns)) {
    ReportError(std::move(callback),
                "Missing or incorrect xmlns on gupdate tag");
    return;
  }

  if (!IsXmlElementNamed(root, GetXmlQualifiedName(gupdate_ns, "gupdate"))) {
    ReportError(std::move(callback), "Missing gupdate tag");
    return;
  }

  // Check for the gupdate "protocol" attribute.
  if (GetXmlElementAttribute(root, "protocol") != kExpectedGupdateProtocol) {
    ReportError(std::move(callback),
                std::string("Missing/incorrect protocol on gupdate tag "
                            "(expected '") +
                    kExpectedGupdateProtocol + "')");
    return;
  }

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
    UpdateManifestResult result;
    std::string app_error;
    if (!ParseSingleAppTag(*app, gupdate_ns, &result, &app_error,
                           &prodversionmin_count)) {
      if (!error_msg.empty())
        error_msg += "\r\n";  // Should we have an OS specific EOL?
      error_msg += app_error;
    } else {
      results->list.push_back(result);
    }
  }

  std::move(callback).Run(
      results->list.empty() ? nullptr : std::move(results),
      error_msg.empty() ? base::Optional<std::string>() : error_msg);
}

}  // namespace

UpdateManifestResult::UpdateManifestResult() = default;

UpdateManifestResult::UpdateManifestResult(const UpdateManifestResult& other) =
    default;

UpdateManifestResult::~UpdateManifestResult() = default;

UpdateManifestResults::UpdateManifestResults() = default;

UpdateManifestResults::UpdateManifestResults(
    const UpdateManifestResults& other) = default;

UpdateManifestResults::~UpdateManifestResults() = default;

std::map<std::string, std::vector<const UpdateManifestResult*>>
UpdateManifestResults::GroupByID() const {
  std::map<std::string, std::vector<const UpdateManifestResult*>> groups;
  for (const UpdateManifestResult& update_result : list) {
    groups[update_result.extension_id].push_back(&update_result);
  }
  return groups;
}

void ParseUpdateManifest(const std::string& xml,
                         ParseUpdateManifestCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(callback);
  data_decoder::DataDecoder::ParseXmlIsolated(
      xml, base::BindOnce(&ParseXmlDone, std::move(callback)));
}

}  // namespace extensions
