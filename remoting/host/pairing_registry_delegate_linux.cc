// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/pairing_registry_delegate_linux.h"

#include <memory>
#include <utility>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/functional/bind.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_string_value_serializer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "remoting/host/branding.h"

namespace {

// The pairing registry path relative to the configuration directory.
const char kRegistryDirectory[] = "paired-clients";

const char kPairingFilenameFormat[] = "%s.json";
const char kPairingFilenamePattern[] = "*.json";

}  // namespace

namespace remoting {

using protocol::PairingRegistry;

PairingRegistryDelegateLinux::PairingRegistryDelegateLinux() = default;

PairingRegistryDelegateLinux::~PairingRegistryDelegateLinux() = default;

base::Value::List PairingRegistryDelegateLinux::LoadAll() {
  base::Value::List pairings;

  // Enumerate all pairing files in the pairing registry.
  base::FilePath registry_path = GetRegistryPath();
  base::FileEnumerator enumerator(registry_path, false,
                                  base::FileEnumerator::FILES,
                                  kPairingFilenamePattern);
  for (base::FilePath pairing_file = enumerator.Next(); !pairing_file.empty();
       pairing_file = enumerator.Next()) {
    // Read the JSON containing pairing data.
    JSONFileValueDeserializer deserializer(pairing_file);
    int error_code;
    std::string error_message;
    std::unique_ptr<base::Value> pairing_json =
        deserializer.Deserialize(&error_code, &error_message);
    if (!pairing_json) {
      LOG(WARNING) << "Failed to load '" << pairing_file.value() << "' ("
                   << error_code << ").";
      continue;
    }

    pairings.Append(base::Value::FromUniquePtrValue(std::move(pairing_json)));
  }

  return pairings;
}

bool PairingRegistryDelegateLinux::DeleteAll() {
  // Delete all pairing files in the pairing registry.
  base::FilePath registry_path = GetRegistryPath();
  base::FileEnumerator enumerator(registry_path, false,
                                  base::FileEnumerator::FILES,
                                  kPairingFilenamePattern);

  bool success = true;
  for (base::FilePath pairing_file = enumerator.Next(); !pairing_file.empty();
       pairing_file = enumerator.Next()) {
    success = success && base::DeleteFile(pairing_file);
  }

  return success;
}

PairingRegistry::Pairing PairingRegistryDelegateLinux::Load(
    const std::string& client_id) {
  base::FilePath registry_path = GetRegistryPath();
  base::FilePath pairing_file = registry_path.Append(
      base::StringPrintf(kPairingFilenameFormat, client_id.c_str()));

  JSONFileValueDeserializer deserializer(pairing_file);
  int error_code;
  std::string error_message;
  std::unique_ptr<base::Value> pairing =
      deserializer.Deserialize(&error_code, &error_message);
  if (!pairing) {
    LOG(WARNING) << "Failed to load pairing information: " << error_message
                 << " (" << error_code << ").";
    return PairingRegistry::Pairing();
  }

  if (!pairing->is_dict()) {
    LOG(WARNING) << "Failed to parse pairing information: not a dictionary.";
    return PairingRegistry::Pairing();
  }

  return PairingRegistry::Pairing::CreateFromValue(pairing->GetDict());
}

bool PairingRegistryDelegateLinux::Save(
    const PairingRegistry::Pairing& pairing) {
  base::FilePath registry_path = GetRegistryPath();
  base::File::Error error;
  if (!base::CreateDirectoryAndGetError(registry_path, &error)) {
    LOG(ERROR) << "Could not create pairing registry directory: " << error;
    return false;
  }

  std::string pairing_json;
  JSONStringValueSerializer serializer(&pairing_json);
  if (!serializer.Serialize(pairing.ToValue())) {
    LOG(ERROR) << "Failed to serialize pairing data for "
               << pairing.client_id();
    return false;
  }

  base::FilePath pairing_file = registry_path.Append(
      base::StringPrintf(kPairingFilenameFormat, pairing.client_id().c_str()));
  if (!base::ImportantFileWriter::WriteFileAtomically(pairing_file,
                                                      pairing_json)) {
    LOG(ERROR) << "Could not save pairing data for " << pairing.client_id();
    return false;
  }

  return true;
}

bool PairingRegistryDelegateLinux::Delete(const std::string& client_id) {
  base::FilePath registry_path = GetRegistryPath();
  base::FilePath pairing_file = registry_path.Append(
      base::StringPrintf(kPairingFilenameFormat, client_id.c_str()));

  return base::DeleteFile(pairing_file);
}

base::FilePath PairingRegistryDelegateLinux::GetRegistryPath() {
  if (!registry_path_for_testing_.empty()) {
    return registry_path_for_testing_;
  }

  base::FilePath config_dir = remoting::GetConfigDir();
  return config_dir.Append(kRegistryDirectory);
}

void PairingRegistryDelegateLinux::SetRegistryPathForTesting(
    const base::FilePath& registry_path) {
  registry_path_for_testing_ = registry_path;
}

std::unique_ptr<PairingRegistry::Delegate> CreatePairingRegistryDelegate() {
  return std::make_unique<PairingRegistryDelegateLinux>();
}

}  // namespace remoting
