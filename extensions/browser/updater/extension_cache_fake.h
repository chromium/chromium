// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_UPDATER_EXTENSION_CACHE_FAKE_H_
#define EXTENSIONS_BROWSER_UPDATER_EXTENSION_CACHE_FAKE_H_

#include <map>
#include <set>
#include <string>

#include "extensions/browser/updater/extension_cache.h"

namespace extensions {

// Fake implementation of extensions ExtensionCache that can be used in tests.
class ExtensionCacheFake : public ExtensionCache {
 public:
  ExtensionCacheFake();

  ExtensionCacheFake(const ExtensionCacheFake&) = delete;
  ExtensionCacheFake& operator=(const ExtensionCacheFake&) = delete;

  ~ExtensionCacheFake() override;

  // Implementation of ExtensionCache.
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

 private:
  typedef std::map<std::string, std::pair<std::string, base::FilePath>> Map;
  // Set of extensions that can be cached.
  std::set<std::string> allowed_extensions_;

  // Map of know extensions.
  Map cache_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_UPDATER_EXTENSION_CACHE_FAKE_H_
