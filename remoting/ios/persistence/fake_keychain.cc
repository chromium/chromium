// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/ios/persistence/fake_keychain.h"

namespace remoting {

static std::string GetKey(Keychain::Key key, const std::string& account) {
  return Keychain::KeyToString(key) + "|" + account;
}

FakeKeychain::FakeKeychain() {}

FakeKeychain::~FakeKeychain() {}

size_t FakeKeychain::GetNumberOfEntries() const {
  return entries_.size();
}

void FakeKeychain::SetData(Key key,
                           const std::string& account,
                           const std::string& data) {
  entries_[GetKey(key, account)] = data;
}

std::string FakeKeychain::GetData(Key key, const std::string& account) const {
  // Note that entries_[key] is not const and will automatically insert default
  // entry to the map.
  const auto data_iterator = entries_.find(GetKey(key, account));
  if (data_iterator == entries_.end()) {
    return "";
  }
  return data_iterator->second;
}

void FakeKeychain::RemoveData(Key key, const std::string& account) {
  auto data_iterator = entries_.find(GetKey(key, account));
  if (data_iterator != entries_.end()) {
    entries_.erase(data_iterator);
  }
}

}  // namespace remoting
