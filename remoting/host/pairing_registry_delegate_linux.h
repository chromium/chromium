// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_PAIRING_REGISTRY_DELEGATE_LINUX_H_
#define REMOTING_HOST_PAIRING_REGISTRY_DELEGATE_LINUX_H_

#include "remoting/protocol/pairing_registry.h"

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"

namespace remoting {

class PairingRegistryDelegateLinux
    : public protocol::PairingRegistry::Delegate {
 public:
  // The pairing registry path relative to the configuration directory.
  static const base::FilePath::CharType kRegistryDirectory[];

  // Determines the registry path and whether unprivileged files should be used
  // based on the current user.
  PairingRegistryDelegateLinux();

  // Used by the native messaging host, which is not run as the network user,
  // and PairingRegistryDelegateLinux() won't work.
  // `.unprivileged.json` files which do not have the shared secret will only
  // be created or read if `use_unprivileged_file` is true.
  PairingRegistryDelegateLinux(const base::FilePath& registry_path,
                               bool use_unprivileged_file);

  PairingRegistryDelegateLinux(const PairingRegistryDelegateLinux&) = delete;
  PairingRegistryDelegateLinux& operator=(const PairingRegistryDelegateLinux&) =
      delete;

  ~PairingRegistryDelegateLinux() override;

  // PairingRegistry::Delegate interface
  base::ListValue LoadAll() override;
  bool DeleteAll() override;
  protocol::PairingRegistry::Pairing Load(
      const std::string& client_id) override;
  bool Save(const protocol::PairingRegistry::Pairing& pairing) override;
  bool Delete(const std::string& client_id) override;

  // Returns the default path to the directory used for loading and saving
  // paired clients.
  static base::FilePath GetDefaultRegistryPath();

 private:
  FRIEND_TEST_ALL_PREFIXES(PairingRegistryDelegateLinuxTest, SaveAndLoad);
  FRIEND_TEST_ALL_PREFIXES(PairingRegistryDelegateLinuxTest, Stateless);

  const base::FilePath registry_path_;
  const bool use_unprivileged_file_ = false;
};

}  // namespace remoting

#endif  // REMOTING_HOST_PAIRING_REGISTRY_DELEGATE_LINUX_H_
