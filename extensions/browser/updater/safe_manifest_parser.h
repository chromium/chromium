// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_UPDATER_SAFE_MANIFEST_PARSER_H_
#define EXTENSIONS_BROWSER_UPDATER_SAFE_MANIFEST_PARSER_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "extensions/common/extension_id.h"
#include "url/gurl.h"

namespace extensions {

// Note: enum used for UMA. Do NOT reorder or remove entries.
// 1) Don't forget to update enums.xml (name: ManifestInvalidError) when adding
// new entries.
// 2) Don't forget to update device_management_backend.proto (name:
// ExtensionInstallReportLogEvent::ManifestInvalidError) when adding new
// entries.
// 3) Don't forget to update ConvertManifestInvalidErrorToProto method in
// ExtensionInstallEventLogCollector.
// Some errors are common for the entire fetched update manifest which
// contains manifests of different extensions, while some errors are per
// extension basis.
enum class ManifestInvalidError {
  // Common for the entire fetched manifest, which contains manifests of
  // different extensions.
  XML_PARSING_FAILED = 0,
  INVALID_XLMNS_ON_GUPDATE_TAG = 1,
  MISSING_GUPDATE_TAG = 2,
  INVALID_PROTOCOL_ON_GUPDATE_TAG = 3,
  // Here onwards we have errors corresponding to a single extension.
  MISSING_APP_ID = 4,
  MISSING_UPDATE_CHECK_TAGS = 5,
  MULTIPLE_UPDATE_CHECK_TAGS = 6,
  INVALID_PRODVERSION_MIN = 7,
  EMPTY_CODEBASE_URL = 8,
  INVALID_CODEBASE_URL = 9,
  MISSING_VERSION_FOR_UPDATE_CHECK = 10,
  INVALID_VERSION = 11,
  BAD_UPDATE_SPECIFICATION = 12,
  BAD_APP_STATUS = 13,
  // Maximum histogram value.
  kMaxValue = BAD_APP_STATUS
};

struct ManifestParseFailure {
  ManifestParseFailure();
  ManifestParseFailure(const ManifestParseFailure& other);
  ManifestParseFailure(std::string error_detail, ManifestInvalidError error);
  ~ManifestParseFailure();

  std::string error_detail;
  ManifestInvalidError error;
};

struct UpdateManifestResult {
  UpdateManifestResult();
  UpdateManifestResult(const UpdateManifestResult& other);
  ~UpdateManifestResult();

  ExtensionId extension_id;
  std::string version;
  std::string browser_min_version;
  std::string app_status;

  // Error occurred while parsing manifest.
  std::optional<ManifestParseFailure> parse_error;

  // Attribute for no update: server may provide additional info about why there
  // is no updates, eg. “bandwidth limit” if client is downloading extensions
  // too aggressive.
  std::optional<std::string> info;

  // Indicates the outcome of the update check.
  std::string status;

  // Attributes for the full update.
  GURL crx_url;
  std::string package_hash;
  int size = 0;
  std::string package_fingerprint;

  // Attributes for the differential update.
  GURL diff_crx_url;
  std::string diff_package_hash;
  int diff_size = 0;
};

inline constexpr int kNoDaystart = -1;
struct UpdateManifestResults {
  UpdateManifestResults();
  UpdateManifestResults(const UpdateManifestResults& other);
  UpdateManifestResults& operator=(const UpdateManifestResults& other);
  ~UpdateManifestResults();

  // Group successful items from |update_list| by |extension_id|.
  std::map<std::string, std::vector<const UpdateManifestResult*>>
  GroupSuccessfulByID() const;

  std::vector<UpdateManifestResult> update_list;
  // This will be >= 0, or kNoDaystart if the <daystart> tag was not present.
  int daystart_elapsed_seconds = kNoDaystart;
};

// Parses an update manifest |xml| safely in a utility process and calls
// |callback| with the results, which will be null on failure. Runs on
// the UI thread.
//
// An update manifest looks like this:
//
// <?xml version="1.0" encoding="UTF-8"?>
// <gupdate xmlns="http://www.google.com/update2/response" protocol="2.0">
//  <daystart elapsed_seconds="300" />
//  <app appid="12345" status="ok">
//   <updatecheck codebase="http://example.com/extension_1.2.3.4.crx"
//                hash="12345" size="9854" status="ok" version="1.2.3.4"
//                prodversionmin="2.0.143.0"
//                codebasediff="http://example.com/diff_1.2.3.4.crx"
//                hashdiff="123" sizediff="101"
//                fp="1.123" />
//  </app>
// </gupdate>
//
// The <daystart> tag contains a "elapsed_seconds" attribute which refers to
// the server's notion of how many seconds it has been since midnight.
//
// The "appid" attribute of the <app> tag refers to the unique id of the
// extension. The "codebase" attribute of the <updatecheck> tag is the url to
// fetch the updated crx file, and the "prodversionmin" attribute refers to
// the minimum version of the chrome browser that the update applies to.

// The diff data members correspond to the differential update package, if
// a differential update is specified in the response.

// The result of parsing one <app> tag in an xml update check manifest.
using ParseUpdateManifestCallback = base::OnceCallback<void(
    std::unique_ptr<UpdateManifestResults> results,
    const std::optional<ManifestParseFailure>& failure)>;
void ParseUpdateManifest(const std::string& xml,
                         ParseUpdateManifestCallback callback);

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_UPDATER_SAFE_MANIFEST_PARSER_H_
