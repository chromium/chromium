// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/ios/persistence/host_pairing_info.h"

#include <memory>

#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "remoting/ios/persistence/remoting_keychain.h"

namespace remoting {

namespace {

const char kPairingIdKey[] = "id";
const char kPairingSecretKey[] = "secret";

static remoting::Keychain* g_keychain =
    remoting::RemotingKeychain::GetInstance();

std::unique_ptr<base::Value> GetHostPairingListForUser(
    const std::string& user_id) {
  std::string data =
      g_keychain->GetData(remoting::Keychain::Key::PAIRING_INFO, user_id);
  if (data.empty()) {
    return std::make_unique<base::DictionaryValue>();
  }
  JSONStringValueDeserializer deserializer(data);
  int error_code = 0;
  std::string error_message;
  std::unique_ptr<base::Value> value =
      deserializer.Deserialize(&error_code, &error_message);
  if (error_code || !error_message.empty()) {
    LOG(ERROR) << "Failed to decode host pairing list. Code: " << error_code
               << " message: " << error_message;
    return std::make_unique<base::DictionaryValue>();
  }
  if (!value->is_dict()) {
    LOG(ERROR) << "Decoded host list is not a dictionary.";
    return std::make_unique<base::DictionaryValue>();
  }
  return value;
}

}  // namespace

HostPairingInfo::HostPairingInfo(const std::string& user_id,
                                 const std::string& host_id,
                                 const std::string& pairing_id,
                                 const std::string& pairing_secret)
    : user_id_(user_id),
      host_id_(host_id),
      pairing_id_(pairing_id),
      pairing_secret_(pairing_secret) {}

HostPairingInfo::HostPairingInfo(const HostPairingInfo&) = default;

HostPairingInfo::HostPairingInfo(HostPairingInfo&&) = default;

HostPairingInfo::~HostPairingInfo() {}

// static
HostPairingInfo HostPairingInfo::GetPairingInfo(const std::string& user_id,
                                                const std::string& host_id) {
  std::unique_ptr<base::Value> host_pairings =
      GetHostPairingListForUser(user_id);

  base::Value* pairing_id = host_pairings->FindPath({host_id, kPairingIdKey});
  base::Value* pairing_secret =
      host_pairings->FindPath({host_id, kPairingSecretKey});
  if (pairing_id && pairing_id->is_string() && pairing_secret &&
      pairing_secret->is_string()) {
    return HostPairingInfo(user_id, host_id, pairing_id->GetString(),
                           pairing_secret->GetString());
  }

  // Pairing not exist or entry corrupted.
  return HostPairingInfo(user_id, host_id, "", "");
}

void HostPairingInfo::Save() {
  std::unique_ptr<base::Value> host_pairings =
      GetHostPairingListForUser(user_id_);

  if (!host_pairings) {
    host_pairings.reset(new base::DictionaryValue());
  }

  host_pairings->SetPath({host_id_, kPairingIdKey}, base::Value(pairing_id_));
  host_pairings->SetPath({host_id_, kPairingSecretKey},
                         base::Value(pairing_secret_));
  std::string json_string;
  JSONStringValueSerializer serializer(&json_string);
  serializer.Serialize(*host_pairings);
  g_keychain->SetData(remoting::Keychain::Key::PAIRING_INFO, user_id_,
                      json_string);
}

// static
void HostPairingInfo::SetKeychainForTesting(Keychain* keychain) {
  g_keychain = keychain ? keychain : remoting::RemotingKeychain::GetInstance();
}

}  // namespace remoting
