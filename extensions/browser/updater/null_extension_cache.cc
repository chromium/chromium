// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/null_extension_cache.h"

#include "base/functional/callback.h"

namespace extensions {

NullExtensionCache::NullExtensionCache() = default;

NullExtensionCache::~NullExtensionCache() = default;

void NullExtensionCache::Start(base::OnceClosure callback) {
  std::move(callback).Run();
}

void NullExtensionCache::Shutdown(base::OnceClosure callback) {
  std::move(callback).Run();
}

void NullExtensionCache::AllowCaching(const std::string& id) {}

bool NullExtensionCache::GetExtension(const std::string& id,
                                      const std::string& expected_hash,
                                      base::FilePath* file_path,
                                      std::string* version) {
  return false;
}

void NullExtensionCache::PutExtension(const std::string& id,
                                      const std::string& expected_hash,
                                      const base::FilePath& file_path,
                                      const std::string& version,
                                      PutExtensionCallback callback) {
  std::move(callback).Run(file_path, true);
}

bool NullExtensionCache::OnInstallFailed(const std::string& id,
                                         const std::string& hash,
                                         const CrxInstallError& error) {
  return false;
}

}  // namespace extensions
