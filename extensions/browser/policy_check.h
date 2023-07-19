// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_POLICY_CHECK_H_
#define EXTENSIONS_BROWSER_POLICY_CHECK_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "extensions/browser/preload_check.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class Extension;

// Checks whether loading this extension is disabled by policy. Synchronously
// calls the callback with the result.
class PolicyCheck : public PreloadCheck {
 public:
  PolicyCheck(content::BrowserContext* context,
              scoped_refptr<const Extension> extension);

  PolicyCheck(const PolicyCheck&) = delete;
  PolicyCheck& operator=(const PolicyCheck&) = delete;

  ~PolicyCheck() override;

  // PreloadCheck:
  void Start(ResultCallback callback) override;
  std::u16string GetErrorMessage() const override;

 private:
  raw_ptr<content::BrowserContext, AcrossTasksDanglingUntriaged> context_;
  std::u16string error_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_POLICY_CHECK_H_
