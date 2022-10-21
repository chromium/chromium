// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_UPDATER_NULL_EXTENSION_CACHE_H_
#define EXTENSIONS_BROWSER_UPDATER_NULL_EXTENSION_CACHE_H_

#include <string>

#include "extensions/browser/updater/extension_cache.h"

namespace extensions {

// Implements a pass-thru (i.e. do-nothing) ExtensionCache.
class NullExtensionCache : public ExtensionCache {
 public:
  NullExtensionCache();

  NullExtensionCache(const NullExtensionCache&) = delete;
  NullExtensionCache& operator=(const NullExtensionCache&) = delete;

  ~NullExtensionCache() override;

  // ExtensionCache implementation.
  void Start(base::OnceClosure callback) override;
  void Shutdown(base::OnceClosure callback) override;
  void AllowCaching(const std::string& id) override;
  bool GetExtension(const std::string& id,
                    const std::string& expected_hash,
                    base::FilePath* file_path,
                    std::string* version) override;
  void PutExtension(const std::string& id,
                    const std::string& expected_hash,
                    const base::FilePath& file_path,
                    const std::string& version,
                    PutExtensionCallback callback) override;
  bool OnInstallFailed(const std::string& id,
                       const std::string& hash,
                       const CrxInstallError& error) override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_UPDATER_NULL_EXTENSION_CACHE_H_
