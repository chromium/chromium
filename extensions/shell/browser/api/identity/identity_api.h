// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_API_IDENTITY_IDENTITY_API_H_
#define EXTENSIONS_SHELL_BROWSER_API_IDENTITY_IDENTITY_API_H_

#include "base/macros.h"
#include "extensions/browser/extension_function.h"

namespace extensions {
namespace shell {

// Stub. See the IDL file for documentation.
class IdentityRemoveCachedAuthTokenFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("identity.removeCachedAuthToken", UNKNOWN)

  IdentityRemoveCachedAuthTokenFunction();

 protected:
  ~IdentityRemoveCachedAuthTokenFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(IdentityRemoveCachedAuthTokenFunction);
};

}  // namespace shell
}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_API_IDENTITY_IDENTITY_API_H_
