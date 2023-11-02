// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_PERSISTENCE_HOST_PAIRING_INFO_H_
#define REMOTING_IOS_PERSISTENCE_HOST_PAIRING_INFO_H_

#include <string>

namespace remoting {

class Keychain;

// A HostPairingInfo contains details to negotiate and maintain a connection
// to a remote Chromoting host.  This is an entity in a backing store.
class HostPairingInfo {
 public:
  HostPairingInfo(const HostPairingInfo&);
  HostPairingInfo(HostPairingInfo&&);
  ~HostPairingInfo();

  // Loads a record from the keychain.
  // If a record does not exist, return a new record with a blank secret.
  static HostPairingInfo GetPairingInfo(const std::string& user_id,
                                        const std::string& host_id);

  // Commit this record to the keychain.
  void Save();

  // Properties supplied by the host server.
  const std::string& user_id() const { return user_id_; }

  const std::string& host_id() const { return host_id_; }

  const std::string& pairing_id() const { return pairing_id_; }
  void set_pairing_id(const std::string& pairing_id) {
    pairing_id_ = pairing_id;
  }

  const std::string& pairing_secret() const { return pairing_secret_; }
  void set_pairing_secret(const std::string& pairing_secret) {
    pairing_secret_ = pairing_secret;
  }

  // The keychain is used to fetch and store host pairing data. The default
  // implementation uses system keychain API. We can supply a custom keychain
  // for testing. Passing null will restore the default keychain.
  static void SetKeychainForTesting(Keychain* keychain);

 private:
  HostPairingInfo(const std::string& user_id,
                  const std::string& host_id,
                  const std::string& pairing_id,
                  const std::string& pairing_secret);

  std::string user_id_;
  std::string host_id_;
  std::string pairing_id_;
  std::string pairing_secret_;
};

}  // namespace remoting

#endif  //  REMOTING_IOS_PERSISTENCE_HOST_PAIRING_INFO_H_
