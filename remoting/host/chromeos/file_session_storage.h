// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMEOS_FILE_SESSION_STORAGE_H_
#define REMOTING_HOST_CHROMEOS_FILE_SESSION_STORAGE_H_

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/values.h"
#include "remoting/host/chromeos/session_storage.h"

namespace remoting {

// Utilities to store CRD session information on the filesystem,
// which allows us to resume the session after a Chrome restart.
// At the time of writing the information is stored in `/run/crd`, which is
// automatically cleared when the ChromeOS device reboots.
class FileSessionStorage : public SessionStorage {
 public:
  FileSessionStorage();
  explicit FileSessionStorage(const base::FilePath& storage_directory);
  FileSessionStorage(const FileSessionStorage&) = default;
  FileSessionStorage& operator=(const FileSessionStorage&) = default;
  FileSessionStorage(FileSessionStorage&&) = default;
  FileSessionStorage& operator=(FileSessionStorage&&) = default;
  ~FileSessionStorage() override = default;

  // `SessionStorage` implementation:
  void StoreSession(const base::Value::Dict& information,
                    base::OnceClosure on_done) override;
  void DeleteSession(base::OnceClosure on_done) override;
  void RetrieveSession(
      base::OnceCallback<void(std::optional<base::Value::Dict>)> on_done)
      override;
  void HasSession(base::OnceCallback<void(bool)> on_done) const override;

  void SetStorageDirectoryForTesting(const base::FilePath& dir);

 private:
  base::FilePath session_file() const;

  base::FilePath storage_directory_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMEOS_FILE_SESSION_STORAGE_H_
