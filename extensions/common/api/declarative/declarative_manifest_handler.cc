// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/declarative/declarative_manifest_handler.h"

#include "extensions/common/api/declarative/declarative_manifest_data.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

DeclarativeManifestHandler::DeclarativeManifestHandler() {
}

DeclarativeManifestHandler::~DeclarativeManifestHandler() {
}

bool DeclarativeManifestHandler::Parse(Extension* extension,
                                       std::u16string* error) {
  const base::Value* event_rules =
      extension->manifest()->FindPath(manifest_keys::kEventRules);
  CHECK(event_rules != nullptr);
  std::unique_ptr<DeclarativeManifestData> data =
      DeclarativeManifestData::FromValue(*event_rules, error);
  if (!data)
    return false;

  extension->SetManifestData(manifest_keys::kEventRules, std::move(data));
  return true;
}

base::span<const char* const> DeclarativeManifestHandler::Keys() const {
  static constexpr const char* kKeys[] = {manifest_keys::kEventRules};
  return kKeys;
}

}  // namespace extensions
