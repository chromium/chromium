// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_API_DECLARATIVE_NET_REQUEST_DNR_MANIFEST_HANDLER_H_
#define EXTENSIONS_COMMON_API_DECLARATIVE_NET_REQUEST_DNR_MANIFEST_HANDLER_H_

#include "extensions/common/manifest_handler.h"

namespace extensions::declarative_net_request {

// Parses the kDeclarativeNetRequestKey manifest key.
class DNRManifestHandler : public ManifestHandler {
 public:
  DNRManifestHandler();

  DNRManifestHandler(const DNRManifestHandler&) = delete;
  DNRManifestHandler& operator=(const DNRManifestHandler&) = delete;

  ~DNRManifestHandler() override;

 private:
  bool Parse(Extension* extension, std::u16string* error) override;
  bool Validate(const Extension* extension,
                std::string* error,
                std::vector<InstallWarning>* warnings) const override;
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions::declarative_net_request

#endif  // EXTENSIONS_COMMON_API_DECLARATIVE_NET_REQUEST_DNR_MANIFEST_HANDLER_H_
