// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_PAIRING_REGISTRY_DELEGATE_WIN_H_
#define REMOTING_HOST_PAIRING_REGISTRY_DELEGATE_WIN_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/win/registry.h"
#include "remoting/protocol/pairing_registry.h"

namespace remoting {

#if defined(OFFICIAL_BUILD)
const wchar_t kPairingRegistryKeyName[] =
    L"SOFTWARE\\Google\\Chrome Remote Desktop\\paired-clients";
#else
const wchar_t kPairingRegistryKeyName[] =
    L"SOFTWARE\\Chromoting\\paired-clients";
#endif

const wchar_t kPairingRegistryClientsKeyName[] = L"clients";
const wchar_t kPairingRegistrySecretsKeyName[] = L"secrets";

// Stores client pairing information in Windows registry. Two separate registry
// keys are used:
//  - |privileged| - contains the shared secrets of all pairings. This key must
//                   be protected by a strong ACL denying access to unprivileged
//                   code.
//  - |unprivileged| - contains the rest of pairing state.
//
// Creator of this object is responsible for passing the registry key handles
// with appropriate access. |privileged| may be nullptr if read-only access is
// sufficient. Shared secrets will not be returned in such a case.
class PairingRegistryDelegateWin : public protocol::PairingRegistry::Delegate {
 public:
  PairingRegistryDelegateWin();

  PairingRegistryDelegateWin(const PairingRegistryDelegateWin&) = delete;
  PairingRegistryDelegateWin& operator=(const PairingRegistryDelegateWin&) =
      delete;

  ~PairingRegistryDelegateWin() override;

  // Passes the root keys to be used to access the pairing registry store.
  // |privileged| is optional and may be nullptr. The caller retains ownership
  // of the passed handles.
  bool SetRootKeys(HKEY privileged, HKEY unprivileged);

  // PairingRegistry::Delegate interface
  base::Value::List LoadAll() override;
  bool DeleteAll() override;
  protocol::PairingRegistry::Pairing Load(
      const std::string& client_id) override;
  bool Save(const protocol::PairingRegistry::Pairing& pairing) override;
  bool Delete(const std::string& client_id) override;

 private:
  base::win::RegKey privileged_;
  base::win::RegKey unprivileged_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_PAIRING_REGISTRY_DELEGATE_WIN_H_
