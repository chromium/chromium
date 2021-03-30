// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_WEB_APP_FILE_HANDLER_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_WEB_APP_FILE_HANDLER_H_

#include <string>

#include "base/containers/span.h"
#include "base/macros.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

struct WebAppFileHandlers : public Extension::ManifestData {
  WebAppFileHandlers();
  ~WebAppFileHandlers() override;

  apps::FileHandlers file_handlers;

  static const apps::FileHandlers* GetWebAppFileHandlers(
      const Extension* extension);
};

// Parses the "web_app_file_handlers" manifest key.
class WebAppFileHandlersParser : public ManifestHandler {
 public:
  WebAppFileHandlersParser();
  ~WebAppFileHandlersParser() override;

  bool Parse(Extension* extension, std::u16string* error) override;

 private:
  base::span<const char* const> Keys() const override;

  DISALLOW_COPY_AND_ASSIGN(WebAppFileHandlersParser);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_WEB_APP_FILE_HANDLER_H_
