// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_DESCRIPTION_INFO_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_DESCRIPTION_INFO_H_

#include <string>

#include "base/containers/span.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

struct DescriptionInfo : public Extension::ManifestData {
  explicit DescriptionInfo(const std::string& description);
  ~DescriptionInfo() override;

  // Return the `extension` description.
  static const std::string& GetDescription(const Extension& extension);

 private:
  std::string description_;
};

// Parses the "description" manifest key.
class DescriptionHandler : public ManifestHandler {
 public:
  DescriptionHandler();

  DescriptionHandler(const DescriptionHandler&) = delete;
  DescriptionHandler& operator=(const DescriptionHandler&) = delete;

  ~DescriptionHandler() override;

  bool Parse(Extension* extension, std::u16string* error) override;

 private:
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_DESCRIPTION_INFO_H_
