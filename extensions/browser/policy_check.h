// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_POLICY_CHECK_H_
#define EXTENSIONS_BROWSER_POLICY_CHECK_H_

#include "base/macros.h"
#include "base/strings/string16.h"
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
  ~PolicyCheck() override;

  // PreloadCheck:
  void Start(ResultCallback callback) override;
  base::string16 GetErrorMessage() const override;

 private:
  content::BrowserContext* context_;
  base::string16 error_;

  DISALLOW_COPY_AND_ASSIGN(PolicyCheck);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_POLICY_CHECK_H_
