// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_API_DECLARATIVE_DECLARATIVE_MANIFEST_DATA_H_
#define EXTENSIONS_COMMON_API_DECLARATIVE_DECLARATIVE_MANIFEST_DATA_H_

#include <string>
#include <vector>

#include "extensions/common/api/events.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

// The parsed form of the "event_rules" manifest entry.
class DeclarativeManifestData : public Extension::ManifestData {
 public:
  using Rule = extensions::api::events::Rule;

  DeclarativeManifestData();

  DeclarativeManifestData(const DeclarativeManifestData&) = delete;
  DeclarativeManifestData& operator=(const DeclarativeManifestData&) = delete;

  ~DeclarativeManifestData() override;

  // Gets the DeclarativeManifestData for |extension|, or NULL if none was
  // specified.
  static DeclarativeManifestData* Get(const Extension* extension);

  // Tries to construct the info based on |value|, as it would have appeared in
  // the manifest. Sets |error| and returns an empty scoped_ptr on failure.
  static std::unique_ptr<DeclarativeManifestData> FromValue(
      const base::Value& value,
      std::u16string* error);

  std::vector<Rule> RulesForEvent(const std::string& event);

 private:
  std::map<std::string, std::vector<Rule>> event_rules_map_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_API_DECLARATIVE_DECLARATIVE_MANIFEST_DATA_H_
