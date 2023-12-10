// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/ios/persistence/host_pairing_info.h"

#include <memory>

#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/values.h"
#include "remoting/ios/persistence/remoting_keychain.h"

namespace remoting {

namespace {

const char kPairingIdKey[] = "id";
const char kPairingSecretKey[] = "secret";

static remoting::Keychain* g_keychain =
    remoting::RemotingKeychain::GetInstance();

base::Value::Dict GetHostPairingListForUser(const std::string& user_id) {
  std::string data =
      g_keychain->GetData(remoting::Keychain::Key::PAIRING_INFO, user_id);
  if (data.empty()) {
    return base::Value::Dict();
  }
  JSONStringValueDeserializer deserializer(data);
  int error_code = 0;
  std::string error_message;
  std::unique_ptr<base::Value> value =
      deserializer.Deserialize(&error_code, &error_message);
  if (error_code || !error_message.empty()) {
    LOG(ERROR) << "Failed to decode host pairing list. Code: " << error_code
               << " message: " << error_message;
    return base::Value::Dict();
  }
  if (!value->is_dict()) {
    LOG(ERROR) << "Decoded host list is not a dictionary.";
    return base::Value::Dict();
  }
  return std::move(*value).TakeDict();
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
  base::Value::Dict host_pairings = GetHostPairingListForUser(user_id);
  base::Value::Dict* host_pairing = host_pairings.FindDict(host_id);
  if (host_pairing) {
    std::string* pairing_id = host_pairing->FindString(kPairingIdKey);
    std::string* pairing_secret = host_pairing->FindString(kPairingSecretKey);
    if (pairing_id && pairing_secret) {
      return HostPairingInfo(user_id, host_id, *pairing_id, *pairing_secret);
    }
  }
  // Pairing not exist or entry corrupted.
  return HostPairingInfo(user_id, host_id, "", "");
}

void HostPairingInfo::Save() {
  base::Value::Dict host_pairings = GetHostPairingListForUser(user_id_);

  base::Value::Dict host_pairing;
  host_pairing.Set(kPairingIdKey, pairing_id_);
  host_pairing.Set(kPairingSecretKey, pairing_secret_);

  host_pairings.Set(host_id_, std::move(host_pairing));

  std::string json_string;
  JSONStringValueSerializer serializer(&json_string);
  serializer.Serialize(host_pairings);
  g_keychain->SetData(remoting::Keychain::Key::PAIRING_INFO, user_id_,
                      json_string);
}

// static
void HostPairingInfo::SetKeychainForTesting(Keychain* keychain) {
  g_keychain = keychain ? keychain : remoting::RemotingKeychain::GetInstance();
}

}  // namespace remoting
