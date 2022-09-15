// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_PERSISTENCE_KEYCHAIN_H_
#define REMOTING_IOS_PERSISTENCE_KEYCHAIN_H_

#include <string>
#include <vector>

namespace remoting {

// Interface for the iOS keychain. This allows it to be mocked out in tests.
class Keychain {
 public:
  enum class Key {
    REFRESH_TOKEN,
    PAIRING_INFO,
  };

  static const std::string kUnspecifiedAccount;

  virtual ~Keychain() {}

  // Commits a keychain entry.
  // |account| can be something like user ID or email address. Please use
  // |kUnspecifiedAccount| if your keychain data is not bound to any user
  // account.
  virtual void SetData(Key key,
                       const std::string& account,
                       const std::string& data) = 0;

  // Retrieves the data of a keychain entry that matches the key and account
  // name. Returns an empty data if no matching data is found.
  virtual std::string GetData(Key key, const std::string& account) const = 0;

  // Removes an entry that matches the key and account name. Does nothing if no
  // matching entry is found.
  virtual void RemoveData(Key key, const std::string& account) = 0;

  static std::string KeyToString(Key key);
};

}  // namespace remoting

#endif  // REMOTING_IOS_PERSISTENCE_KEYCHAIN_H_
