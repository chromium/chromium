// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_CROSS_ORIGIN_ISOLATION_INFO_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_CROSS_ORIGIN_ISOLATION_INFO_H_

#include <string>

#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

struct CrossOriginIsolationHeader : public Extension::ManifestData {
  explicit CrossOriginIsolationHeader(std::string value);
  ~CrossOriginIsolationHeader() override;

  // Returns the value specified by the `extension` for the respective header.
  // If the extension didn't specify a value, null is returned.
  static const std::string* GetCrossOriginEmbedderPolicy(
      const Extension& extension);
  static const std::string* GetCrossOriginOpenerPolicy(
      const Extension& extension);

  // Header value specified by the extension.
  const std::string value;
};

// Parses the "cross_origin_embedder_policy" and "cross_origin_opener_policy"
// manifest keys.
class CrossOriginIsolationHandler : public ManifestHandler {
 public:
  CrossOriginIsolationHandler();
  ~CrossOriginIsolationHandler() override;
  CrossOriginIsolationHandler(const CrossOriginIsolationHandler&) = delete;
  CrossOriginIsolationHandler& operator=(const CrossOriginIsolationHandler&) =
      delete;

  bool Parse(Extension* extension, std::u16string* error) override;

 private:
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_CROSS_ORIGIN_ISOLATION_INFO_H_
