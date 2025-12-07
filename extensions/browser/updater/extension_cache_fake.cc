// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/updater/extension_cache_fake.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {

ExtensionCacheFake::ExtensionCacheFake() = default;

ExtensionCacheFake::~ExtensionCacheFake() = default;

void ExtensionCacheFake::Start(base::OnceClosure callback) {
  content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(callback));
}

void ExtensionCacheFake::Shutdown(base::OnceClosure callback) {
  content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(callback));
}

void ExtensionCacheFake::AllowCaching(const std::string& id) {
  allowed_extensions_.insert(id);
}

bool ExtensionCacheFake::GetExtension(const std::string& id,
                                      const std::string& expected_hash,
                                      base::FilePath* file_path,
                                      std::string* version) {
  auto it = cache_.find(id);
  if (it == cache_.end()) {
    return false;
  } else {
    if (version) {
      *version = it->second.first;
    }
    if (file_path) {
      *file_path = it->second.second;
    }
    return true;
  }
}

void ExtensionCacheFake::PutExtension(const std::string& id,
                                      const std::string& expected_hash,
                                      const base::FilePath& file_path,
                                      const std::string& version,
                                      PutExtensionCallback callback) {
  if (base::Contains(allowed_extensions_, id)) {
    cache_[id].first = version;
    cache_[id].second = file_path;
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), file_path, false));
  } else {
    std::move(callback).Run(file_path, true);
  }
}

bool ExtensionCacheFake::OnInstallFailed(const std::string& id,
                                         const std::string& hash,
                                         const CrxInstallError& error) {
  return false;
}

}  // namespace extensions
