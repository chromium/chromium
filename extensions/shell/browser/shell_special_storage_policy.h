// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_SPECIAL_STORAGE_POLICY_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_SPECIAL_STORAGE_POLICY_H_

#include "storage/browser/quota/special_storage_policy.h"

namespace extensions {

// A simple storage policy for app_shell which does not limit storage
// capabilities and aims to be as permissive as possible.
class ShellSpecialStoragePolicy : public storage::SpecialStoragePolicy {
 public:
  ShellSpecialStoragePolicy();

  // storage::SpecialStoragePolicy implementation.
  bool IsStorageProtected(const GURL& origin) override;
  bool IsStorageUnlimited(const GURL& origin) override;
  bool IsStorageDurable(const GURL& origin) override;
  bool IsStorageSessionOnly(const GURL& origin) override;
  bool HasIsolatedStorage(const GURL& origin) override;
  bool HasSessionOnlyOrigins() override;

 protected:
  ~ShellSpecialStoragePolicy() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_SPECIAL_STORAGE_POLICY_H_
