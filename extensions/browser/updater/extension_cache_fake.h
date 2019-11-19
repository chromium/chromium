// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_UPDATER_EXTENSION_CACHE_FAKE_H_
#define EXTENSIONS_BROWSER_UPDATER_EXTENSION_CACHE_FAKE_H_

#include <map>
#include <set>
#include <string>

#include "base/macros.h"
#include "extensions/browser/updater/extension_cache.h"

namespace extensions {

// Fake implementation of extensions ExtensionCache that can be used in tests.
class ExtensionCacheFake : public ExtensionCache {
 public:
  ExtensionCacheFake();
  ~ExtensionCacheFake() override;

  // Implementation of ExtensionCache.
  void Start(const base::Closure& callback) override;
  void Shutdown(const base::Closure& callback) override;
  void AllowCaching(const std::string& id) override;
  bool GetExtension(const std::string& id,
                    const std::string& expected_hash,
                    base::FilePath* file_path,
                    std::string* version) override;
  void PutExtension(const std::string& id,
                    const std::string& expected_hash,
                    const base::FilePath& file_path,
                    const std::string& version,
                    const PutExtensionCallback& callback) override;

 private:
  typedef std::map<std::string, std::pair<std::string, base::FilePath>> Map;
  // Set of extensions that can be cached.
  std::set<std::string> allowed_extensions_;

  // Map of know extensions.
  Map cache_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionCacheFake);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_UPDATER_EXTENSION_CACHE_FAKE_H_
