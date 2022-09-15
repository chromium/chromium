// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_PERSISTENCE_REMOTING_KEYCHAIN_H_
#define REMOTING_IOS_PERSISTENCE_REMOTING_KEYCHAIN_H_

#include "remoting/ios/persistence/keychain.h"

namespace remoting {

// Class to abstract the details of how iOS wants to write to the keychain.
class RemotingKeychain : public Keychain {
 public:
  RemotingKeychain();

  RemotingKeychain(const RemotingKeychain&) = delete;
  RemotingKeychain& operator=(const RemotingKeychain&) = delete;

  ~RemotingKeychain() override;

  static RemotingKeychain* GetInstance();

  // Keychain overrides.
  void SetData(Key key,
               const std::string& account,
               const std::string& data) override;
  std::string GetData(Key key, const std::string& account) const override;
  void RemoveData(Key key, const std::string& account) override;

  void SetServicePrefixForTesting(const std::string& service_prefix);

 private:
  std::string KeyToService(Key key) const;

  std::string service_prefix_;
};

}  // namespace remoting

#endif  //  REMOTING_IOS_PERSISTENCE_REMOTING_KEYCHAIN_H_
