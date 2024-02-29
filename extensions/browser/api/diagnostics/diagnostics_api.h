// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DIAGNOSTICS_DIAGNOSTICS_API_H_
#define EXTENSIONS_BROWSER_API_DIAGNOSTICS_DIAGNOSTICS_API_H_

#include <memory>
#include <optional>
#include <string>

#include "extensions/browser/extension_function.h"
#include "extensions/common/api/diagnostics.h"

namespace extensions {

class DiagnosticsSendPacketFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("diagnostics.sendPacket", DIAGNOSTICS_SENDPACKET)

  DiagnosticsSendPacketFunction();

 protected:
  ~DiagnosticsSendPacketFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  void OnTestICMPCompleted(std::optional<std::string> status);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DIAGNOSTICS_DIAGNOSTICS_API_H_
