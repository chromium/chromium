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
  PairingRegistryDelegateLinux();

  PairingRegistryDelegateLinux(const PairingRegistryDelegateLinux&) = delete;
  PairingRegistryDelegateLinux& operator=(const PairingRegistryDelegateLinux&) =
      delete;

  ~PairingRegistryDelegateLinux() override;

  // PairingRegistry::Delegate interface
  base::Value::List LoadAll() override;
  bool DeleteAll() override;
  protocol::PairingRegistry::Pairing Load(
      const std::string& client_id) override;
  bool Save(const protocol::PairingRegistry::Pairing& pairing) override;
  bool Delete(const std::string& client_id) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(PairingRegistryDelegateLinuxTest, SaveAndLoad);
  FRIEND_TEST_ALL_PREFIXES(PairingRegistryDelegateLinuxTest, Stateless);

  // Return the path to the directory to use for loading and saving paired
  // clients.
  base::FilePath GetRegistryPath();

  // For testing purposes, set the path returned by |GetRegistryPath()|.
  void SetRegistryPathForTesting(const base::FilePath& registry_path);

  base::FilePath registry_path_for_testing_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_PAIRING_REGISTRY_DELEGATE_LINUX_H_
