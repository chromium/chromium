// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/pairing_registry_delegate_win.h"

#include <windows.h>

#include <optional>
#include <utility>

#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "base/win/registry.h"

namespace remoting {

namespace {

// Duplicates a registry key handle (returned by RegCreateXxx/RegOpenXxx).
// The returned handle cannot be inherited and has the same permissions as
// the source one.
bool DuplicateKeyHandle(HKEY source, base::win::RegKey* dest) {
  HANDLE handle;
  if (!DuplicateHandle(GetCurrentProcess(),
                       source,
                       GetCurrentProcess(),
                       &handle,
                       0,
                       FALSE,
                       DUPLICATE_SAME_ACCESS)) {
    PLOG(ERROR) << "Failed to duplicate a registry key handle";
    return false;
  }

  dest->Set(reinterpret_cast<HKEY>(handle));
  return true;
}

// Reads value |value_name| from |key| as a JSON string and returns it as
// |base::Value|.
std::optional<base::Value::Dict> ReadValue(const base::win::RegKey& key,
                                           const wchar_t* value_name) {
  // presubmit: allow wstring
  std::wstring value_json;
  LONG result = key.ReadValue(value_name, &value_json);
  if (result != ERROR_SUCCESS) {
    SetLastError(result);
    PLOG(ERROR) << "Cannot read value '" << value_name << "'";
    return std::nullopt;
  }

  // Parse the value.
  std::string value_json_utf8 = base::WideToUTF8(value_json);
  JSONStringValueDeserializer deserializer(value_json_utf8);
  int error_code;
  std::string error_message;
  std::unique_ptr<base::Value> value =
      deserializer.Deserialize(&error_code, &error_message);
  if (!value) {
    LOG(ERROR) << "Failed to parse '" << value_name << "': " << error_message
               << " (" << error_code << ").";
    return std::nullopt;
  }

  if (!value->is_dict()) {
    LOG(ERROR) << "Failed to parse '" << value_name << "': not a dictionary.";
    return std::nullopt;
  }

  return std::move(*value).TakeDict();
}

// Serializes |value| into a JSON string and writes it as value |value_name|
// under |key|.
bool WriteValue(base::win::RegKey& key,
                const wchar_t* value_name,
                const base::Value::Dict& value) {
  std::string value_json_utf8;
  JSONStringValueSerializer serializer(&value_json_utf8);
  if (!serializer.Serialize(value)) {
    LOG(ERROR) << "Failed to serialize '" << value_name << "'";
    return false;
  }

  // presubmit: allow wstring
  std::wstring value_json = base::UTF8ToWide(value_json_utf8);
  LONG result = key.WriteValue(value_name, value_json.c_str());
  if (result != ERROR_SUCCESS) {
    SetLastError(result);
    PLOG(ERROR) << "Cannot write value '" << value_name << "'";
    return false;
  }

  return true;
}

}  // namespace

using protocol::PairingRegistry;

PairingRegistryDelegateWin::PairingRegistryDelegateWin() {}

PairingRegistryDelegateWin::~PairingRegistryDelegateWin() {}

bool PairingRegistryDelegateWin::SetRootKeys(HKEY privileged,
                                             HKEY unprivileged) {
  DCHECK(!privileged_.Valid());
  DCHECK(!unprivileged_.Valid());
  DCHECK(unprivileged);

  if (!DuplicateKeyHandle(unprivileged, &unprivileged_)) {
    return false;
  }

  if (privileged) {
    if (!DuplicateKeyHandle(privileged, &privileged_)) {
      return false;
    }
  }

  return true;
}

base::Value::List PairingRegistryDelegateWin::LoadAll() {
  base::Value::List pairings;

  // Enumerate and parse all values under the unprivileged key.
  DWORD count = unprivileged_.GetValueCount().value_or(0);
  for (DWORD index = 0; index < count; ++index) {
    // presubmit: allow wstring
    std::wstring value_name;
    LONG result = unprivileged_.GetValueNameAt(index, &value_name);
    if (result != ERROR_SUCCESS) {
      SetLastError(result);
      PLOG(ERROR) << "Cannot get the name of value " << index;
      continue;
    }

    PairingRegistry::Pairing pairing = Load(base::WideToUTF8(value_name));
    if (pairing.is_valid()) {
      pairings.Append(pairing.ToValue());
    }
  }

  return pairings;
}

bool PairingRegistryDelegateWin::DeleteAll() {
  if (!privileged_.Valid()) {
    LOG(ERROR) << "Cannot delete pairings: the delegate is read-only.";
    return false;
  }

  // Enumerate and delete the values in the privileged and unprivileged keys
  // separately in case they get out of sync.
  bool success = true;
  DWORD count = unprivileged_.GetValueCount().value_or(0);
  while (count > 0) {
    // presubmit: allow wstring
    std::wstring value_name;
    LONG result = unprivileged_.GetValueNameAt(0, &value_name);
    if (result == ERROR_SUCCESS) {
      result = unprivileged_.DeleteValue(value_name.c_str());
    }

    success = success && (result == ERROR_SUCCESS);
    count = unprivileged_.GetValueCount().value_or(0);
  }

  count = privileged_.GetValueCount().value_or(0);
  while (count > 0) {
    // presubmit: allow wstring
    std::wstring value_name;
    LONG result = privileged_.GetValueNameAt(0, &value_name);
    if (result == ERROR_SUCCESS) {
      result = privileged_.DeleteValue(value_name.c_str());
    }

    success = success && (result == ERROR_SUCCESS);
    count = privileged_.GetValueCount().value_or(0);
  }

  return success;
}

PairingRegistry::Pairing PairingRegistryDelegateWin::Load(
    const std::string& client_id) {
  // presubmit: allow wstring
  std::wstring value_name = base::UTF8ToWide(client_id);

  // Read unprivileged fields first.
  std::optional<base::Value::Dict> pairing =
      ReadValue(unprivileged_, value_name.c_str());
  if (!pairing) {
    return PairingRegistry::Pairing();
  }

  // Read the shared secret.
  if (privileged_.Valid()) {
    std::optional<base::Value::Dict> secret =
        ReadValue(privileged_, value_name.c_str());
    if (!secret) {
      return PairingRegistry::Pairing();
    }

    // Merge the two dictionaries.
    pairing->Merge(std::move(*secret));
  }

  return PairingRegistry::Pairing::CreateFromValue(*pairing);
}

bool PairingRegistryDelegateWin::Save(const PairingRegistry::Pairing& pairing) {
  if (!privileged_.Valid()) {
    LOG(ERROR) << "Cannot save pairing entry '" << pairing.client_id()
               << "': the pairing registry privileged key is invalid.";
    return false;
  }

  // Convert pairing to JSON.
  base::Value::Dict pairing_json = pairing.ToValue();

  // Extract the shared secret to a separate dictionary.
  std::optional<base::Value> secret_key =
      pairing_json.Extract(PairingRegistry::kSharedSecretKey);
  CHECK(secret_key.has_value());
  base::Value::Dict secret_json;
  secret_json.Set(PairingRegistry::kSharedSecretKey, std::move(*secret_key));

  // presubmit: allow wstring
  std::wstring value_name = base::UTF8ToWide(pairing.client_id());

  // Write pairing to the registry.
  if (!WriteValue(privileged_, value_name.c_str(), std::move(secret_json)) ||
      !WriteValue(unprivileged_, value_name.c_str(), std::move(pairing_json))) {
    return false;
  }

  return true;
}

bool PairingRegistryDelegateWin::Delete(const std::string& client_id) {
  if (!privileged_.Valid()) {
    LOG(ERROR) << "Cannot delete pairing entry '" << client_id
               << "': the delegate is read-only.";
    return false;
  }

  // presubmit: allow wstring
  std::wstring value_name = base::UTF8ToWide(client_id);
  LONG result = privileged_.DeleteValue(value_name.c_str());
  if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND &&
      result != ERROR_PATH_NOT_FOUND) {
    SetLastError(result);
    PLOG(ERROR) << "Cannot delete pairing entry '" << client_id << "'";
    return false;
  }

  result = unprivileged_.DeleteValue(value_name.c_str());
  if (result != ERROR_SUCCESS && result != ERROR_FILE_NOT_FOUND &&
      result != ERROR_PATH_NOT_FOUND) {
    SetLastError(result);
    PLOG(ERROR) << "Cannot delete pairing entry '" << client_id << "'";
    return false;
  }

  return true;
}

std::unique_ptr<PairingRegistry::Delegate> CreatePairingRegistryDelegate() {
  return std::make_unique<PairingRegistryDelegateWin>();
}

}  // namespace remoting
