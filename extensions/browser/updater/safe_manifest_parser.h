// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_UPDATER_SAFE_MANIFEST_PARSER_H_
#define EXTENSIONS_BROWSER_UPDATER_SAFE_MANIFEST_PARSER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/optional.h"
#include "url/gurl.h"

namespace extensions {

struct UpdateManifestResult {
  UpdateManifestResult();
  UpdateManifestResult(const UpdateManifestResult& other);
  ~UpdateManifestResult();

  std::string extension_id;
  std::string version;
  std::string browser_min_version;

  // Attribute for no update: server may provide additional info about why there
  // is no updates, eg. “bandwidth limit” if client is downloading extensions
  // too aggressive.
  base::Optional<std::string> info;

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

constexpr int kNoDaystart = -1;
struct UpdateManifestResults {
  UpdateManifestResults();
  UpdateManifestResults(const UpdateManifestResults& other);
  UpdateManifestResults& operator=(const UpdateManifestResults& other);
  ~UpdateManifestResults();

  // Group |list| by |extension_id|.
  std::map<std::string, std::vector<const UpdateManifestResult*>> GroupByID()
      const;

  std::vector<UpdateManifestResult> list;
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
using ParseUpdateManifestCallback =
    base::OnceCallback<void(std::unique_ptr<UpdateManifestResults> results,
                            const base::Optional<std::string>& error)>;
void ParseUpdateManifest(const std::string& xml,
                         ParseUpdateManifestCallback callback);

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_UPDATER_SAFE_MANIFEST_PARSER_H_
