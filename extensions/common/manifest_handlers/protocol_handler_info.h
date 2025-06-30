// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_PROTOCOL_HANDLER_INFO_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_PROTOCOL_HANDLER_INFO_H_

#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "components/services/app_service/public/cpp/protocol_handler_info.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

using ProtocolHandlersInfo = std::vector<apps::ProtocolHandlerInfo>;

struct ProtocolHandlers : public Extension::ManifestData {
  ProtocolHandlers();
  ~ProtocolHandlers() override;

  ProtocolHandlersInfo protocol_handlers;

  static const ProtocolHandlersInfo* GetProtocolHandlers(
      const Extension& extension);
};

// Parses the "protocol_handlers" manifest key.
class ProtocolHandlersParser : public ManifestHandler {
 public:
  ProtocolHandlersParser();

  ProtocolHandlersParser(const ProtocolHandlersParser&) = delete;
  ProtocolHandlersParser& operator=(const ProtocolHandlersParser&) = delete;

  ~ProtocolHandlersParser() override;

  bool Parse(Extension* extension, std::u16string* error) override;

 private:
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_PROTOCOL_HANDLER_INFO_H_
