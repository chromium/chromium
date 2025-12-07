// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_UPDATE_INSTALL_GATE_H_
#define EXTENSIONS_BROWSER_UPDATE_INSTALL_GATE_H_

#include "base/memory/raw_ptr.h"
#include "extensions/browser/install_gate.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {
// Delays an extension update if the old version is not idle.
class UpdateInstallGate : public InstallGate {
 public:
  explicit UpdateInstallGate(content::BrowserContext* browser_context);

  UpdateInstallGate(const UpdateInstallGate&) = delete;
  UpdateInstallGate& operator=(const UpdateInstallGate&) = delete;

  // InstallGate:
  Action ShouldDelay(const Extension* extension,
                     bool install_immediately) override;

 private:
  // Not owned.
  const raw_ptr<content::BrowserContext, DanglingUntriaged> browser_context_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_UPDATE_INSTALL_GATE_H_
