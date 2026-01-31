// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_SERVICES_WEB_APP_ORIGIN_ASSOCIATION_WEB_APP_ORIGIN_ASSOCIATION_PARSER_H_
#define COMPONENTS_WEBAPPS_SERVICES_WEB_APP_ORIGIN_ASSOCIATION_WEB_APP_ORIGIN_ASSOCIATION_PARSER_H_

#include <string>
#include <vector>

#include "base/types/expected.h"
#include "url/gurl.h"

namespace url {
class Origin;
}  // namespace url

namespace webapps {
extern const char kWebAppOriginAssociationParserFormatError[];
extern const char kInvalidManifestId[];
extern const char kInvalidValueType[];
extern const char kInvalidScopeUrl[];

struct AssociatedWebApp {
  GURL web_app_identity;
  GURL scope;
  bool allow_migration = false;

  bool operator==(const AssociatedWebApp&) const = default;
};

struct ParsedAssociations {
  ParsedAssociations();
  ~ParsedAssociations();

  ParsedAssociations(const ParsedAssociations&);
  ParsedAssociations& operator=(const ParsedAssociations&);

  ParsedAssociations(ParsedAssociations&&);
  ParsedAssociations& operator=(ParsedAssociations&&);

  std::vector<AssociatedWebApp> apps;
  std::vector<std::string> warnings;
};

// Handles the logic of parsing the web app origin association file from a
// string as described in the "Scope Extensions for Web Apps" explainer:
// https://github.com/WICG/manifest-incubations/blob/gh-pages/scope_extensions-explainer.md
base::expected<ParsedAssociations, std::string> ParseWebAppOriginAssociations(
    const std::string& data,
    const url::Origin& origin);

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_SERVICES_WEB_APP_ORIGIN_ASSOCIATION_WEB_APP_ORIGIN_ASSOCIATION_PARSER_H_
