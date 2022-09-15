// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_PERSISTENCE_FAKE_KEYCHAIN_H_
#define REMOTING_IOS_PERSISTENCE_FAKE_KEYCHAIN_H_

#include <map>

#include "remoting/ios/persistence/keychain.h"

namespace remoting {

// A fake keychain implementation that stores entries in memory. Supposed to be
// used in tests.
class FakeKeychain : public Keychain {
 public:
  FakeKeychain();

  FakeKeychain(const FakeKeychain&) = delete;
  FakeKeychain& operator=(const FakeKeychain&) = delete;

  ~FakeKeychain() override;

  size_t GetNumberOfEntries() const;

  // Keychain overrides.
  void SetData(Key key,
               const std::string& account,
               const std::string& data) override;
  std::string GetData(Key key, const std::string& account) const override;
  void RemoveData(Key key, const std::string& account) override;

 private:
  std::map<std::string, std::string> entries_;
};

}  // namespace remoting

#endif  // REMOTING_IOS_PERSISTENCE_FAKE_KEYCHAIN_H_
