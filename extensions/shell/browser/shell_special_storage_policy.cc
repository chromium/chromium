// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_special_storage_policy.h"

#include "base/bind.h"
#include "base/callback.h"

namespace extensions {

ShellSpecialStoragePolicy::ShellSpecialStoragePolicy() {
}

ShellSpecialStoragePolicy::~ShellSpecialStoragePolicy() {
}

bool ShellSpecialStoragePolicy::IsStorageProtected(const GURL& origin) {
  return true;
}

bool ShellSpecialStoragePolicy::IsStorageUnlimited(const GURL& origin) {
  return true;
}

bool ShellSpecialStoragePolicy::IsStorageDurable(const GURL& origin) {
  // The plan is to forbid extensions from acquiring the durable storage
  // permission because they can specify 'unlimitedStorage' in the manifest.
  return false;
}

bool ShellSpecialStoragePolicy::IsStorageSessionOnly(const GURL& origin) {
  return false;
}

bool ShellSpecialStoragePolicy::HasSessionOnlyOrigins() {
  return false;
}

network::DeleteCookiePredicate
ShellSpecialStoragePolicy::CreateDeleteCookieOnExitPredicate() {
  return network::DeleteCookiePredicate();
}

bool ShellSpecialStoragePolicy::HasIsolatedStorage(const GURL& origin) {
  return false;
}

}  // namespace extensions
