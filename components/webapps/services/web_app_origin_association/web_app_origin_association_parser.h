// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_SERVICES_WEB_APP_ORIGIN_ASSOCIATION_WEB_APP_ORIGIN_ASSOCIATION_PARSER_H_
#define COMPONENTS_WEBAPPS_SERVICES_WEB_APP_ORIGIN_ASSOCIATION_WEB_APP_ORIGIN_ASSOCIATION_PARSER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/values.h"
#include "components/webapps/services/web_app_origin_association/public/mojom/web_app_origin_association_parser.mojom.h"

namespace base {
class Value;
}  // namespace base

namespace webapps {

// Handles the logic of parsing the web app origin association file from a
// string as described in the "Scope Extensions for Web Apps" explainer:
// https://github.com/WICG/manifest-incubations/blob/gh-pages/scope_extensions-explainer.md
class WebAppOriginAssociationParser {
 public:
  WebAppOriginAssociationParser();
  WebAppOriginAssociationParser& operator=(
      const WebAppOriginAssociationParser&) = delete;
  WebAppOriginAssociationParser(const WebAppOriginAssociationParser&) = delete;
  ~WebAppOriginAssociationParser();

  mojom::WebAppOriginAssociationPtr Parse(const std::string& data);
  bool failed() const;
  // Return errors and clear up |errors_|.
  std::vector<mojom::WebAppOriginAssociationErrorPtr> GetErrors();

 private:
  std::vector<mojom::AssociatedWebAppPtr> ParseAssociatedWebApps(
      const base::Value::Dict& root_dict);
  std::optional<mojom::AssociatedWebAppPtr> ParseAssociatedWebApp(
      const base::Value::Dict& app_dict);
  void AddErrorInfo(const std::string& error_msg,
                    int error_line = 0,
                    int error_column = 0);

  // Set to true if |data| cannot be parsed, or the parsed value is not a valid
  // json object.
  bool failed_ = false;
  // Stores errorrs. Cleared up once GetErrors is called.
  std::vector<mojom::WebAppOriginAssociationErrorPtr> errors_;
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_SERVICES_WEB_APP_ORIGIN_ASSOCIATION_WEB_APP_ORIGIN_ASSOCIATION_PARSER_H_
