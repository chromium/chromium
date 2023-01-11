// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_UPDATER_EXTENSION_CACHE_H_
#define EXTENSIONS_BROWSER_UPDATER_EXTENSION_CACHE_H_

#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"

namespace extensions {

class CrxInstallError;

// ExtensionCache interface that caches extensions .crx files to share them
// between multiple users and profiles on the machine.
class ExtensionCache {
 public:
  // Callback that is invoked when the file placed when PutExtension done.
  using PutExtensionCallback =
      base::OnceCallback<void(const base::FilePath& file_path,
                              bool file_ownership_passed)>;

  ExtensionCache() = default;

  ExtensionCache(const ExtensionCache&) = delete;
  ExtensionCache& operator=(const ExtensionCache&) = delete;

  virtual ~ExtensionCache() = default;

  // Initialize cache in background. The |callback| is called when cache ready.
  // Can be called multiple times. The |callback| can be called immediately if
  // cache is ready.
  virtual void Start(base::OnceClosure callback) = 0;

  // Shut down the cache. Must be called at most once on browser shutdown.
  virtual void Shutdown(base::OnceClosure callback) = 0;

  // Allow caching for the extension with given |id|. User specific extensions
  // should not be cached for privacy reasons. But default apps including policy
  // configured can be cached. Can be called before Init.
  virtual void AllowCaching(const std::string& id) = 0;

  // If extension with |id| exists in the cache, returns |true|, |file_path| and
  // |version| for the extension. Extension will be marked as used with current
  // timestamp.
  virtual bool GetExtension(const std::string& id,
                            const std::string& expected_hash,
                            base::FilePath* file_path,
                            std::string* version) = 0;

  // Put extension with |id| and |version| into local cache. Older version in
  // the cache will removed be on next run so it can be safely used. Extension
  // will be marked as used with current timestamp. The file will be available
  // via GetExtension when |callback| is called. Original |file_path| won't be
  // deleted from the disk. There is no guarantee that |callback| will be
  // called.
  virtual void PutExtension(const std::string& id,
                            const std::string& expected_hash,
                            const base::FilePath& file_path,
                            const std::string& version,
                            PutExtensionCallback callback) = 0;

  // Should be called when CrxInstaller fails to install an extension with the
  // given id and hash. Allows the cache to respond by removing the
  // corresponding cached entry when applicable.
  //
  // Returns whether the extension was removed in response to the given error.
  virtual bool OnInstallFailed(const std::string& id,
                               const std::string& hash,
                               const CrxInstallError& error) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_UPDATER_EXTENSION_CACHE_H_
