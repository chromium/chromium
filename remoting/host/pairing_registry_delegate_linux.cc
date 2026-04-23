// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/pairing_registry_delegate_linux.h"

#include <unistd.h>

#include <memory>
#include <optional>
#include <string>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/cstring_view.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "remoting/base/branding.h"
#include "remoting/base/passwd_utils.h"
#include "remoting/base/username.h"

namespace {

constexpr base::cstring_view kPairingFilenamePattern = "*.json";
constexpr base::cstring_view kPrivilegedSuffix = ".json";
constexpr base::cstring_view kUnprivilegedSuffix = ".unprivileged.json";

}  // namespace

namespace remoting {

using protocol::PairingRegistry;

// static
const base::FilePath::CharType
    PairingRegistryDelegateLinux::kRegistryDirectory[] = "paired-clients";

PairingRegistryDelegateLinux::PairingRegistryDelegateLinux()
    : PairingRegistryDelegateLinux(GetDefaultRegistryPath(),
                                   /*use_unprivileged_file=*/GetUsername() ==
                                       GetNetworkProcessUsername()) {}

PairingRegistryDelegateLinux::PairingRegistryDelegateLinux(
    const base::FilePath& registry_path,
    bool use_unprivileged_file)
    : registry_path_(registry_path),
      use_unprivileged_file_(use_unprivileged_file) {}

PairingRegistryDelegateLinux::~PairingRegistryDelegateLinux() = default;

base::ListValue PairingRegistryDelegateLinux::LoadAll() {
  base::ListValue pairings;

  // Enumerate all pairing files in the pairing registry.
  base::FileEnumerator enumerator(registry_path_, false,
                                  base::FileEnumerator::FILES,
                                  kPairingFilenamePattern.c_str());

  std::set<std::string> client_ids;
  for (base::FilePath pairing_file = enumerator.Next(); !pairing_file.empty();
       pairing_file = enumerator.Next()) {
    base::FilePath::StringType filename = pairing_file.BaseName().value();
    if (filename.empty()) {
      continue;
    }

    std::string client_id;
    if (base::EndsWith(filename, kUnprivilegedSuffix)) {
      if (!use_unprivileged_file_) {
        LOG(WARNING) << "Ignored unprivileged file: " << filename;
        continue;
      }
      client_id =
          filename.substr(0, filename.size() - kUnprivilegedSuffix.size());
    } else if (base::EndsWith(filename, kPrivilegedSuffix)) {
      client_id =
          filename.substr(0, filename.size() - kPrivilegedSuffix.size());
    } else {
      continue;
    }
    client_ids.insert(client_id);
  }

  for (const auto& client_id : client_ids) {
    PairingRegistry::Pairing pairing = Load(client_id);
    if (pairing.is_valid()) {
      pairings.Append(pairing.ToValue());
    }
  }

  return pairings;
}

bool PairingRegistryDelegateLinux::DeleteAll() {
  // Delete all pairing files in the pairing registry.
  base::FileEnumerator enumerator(registry_path_, false,
                                  base::FileEnumerator::FILES,
                                  kPairingFilenamePattern.c_str());

  bool success = true;
  for (base::FilePath pairing_file = enumerator.Next(); !pairing_file.empty();
       pairing_file = enumerator.Next()) {
    success = base::DeleteFile(pairing_file) && success;
  }

  return success;
}

PairingRegistry::Pairing PairingRegistryDelegateLinux::Load(
    const std::string& client_id) {
  base::FilePath pairing_file =
      registry_path_.Append(client_id + kPrivilegedSuffix);

  JSONFileValueDeserializer deserializer(pairing_file);
  int error_code;
  std::string error_message;
  // Try reading the privileged pairing file first.
  std::unique_ptr<base::Value> pairing =
      deserializer.Deserialize(&error_code, &error_message);
  if (!pairing && use_unprivileged_file_ &&
      (error_code == JSONFileValueDeserializer::JSON_ACCESS_DENIED ||
       error_code == JSONFileValueDeserializer::JSON_CANNOT_READ_FILE ||
       error_code == JSONFileValueDeserializer::JSON_NO_SUCH_FILE)) {
    // If it is not readable, then try the unprivileged pairing file.
    base::FilePath unprivileged_pairing_file =
        registry_path_.Append(client_id + kUnprivilegedSuffix);
    JSONFileValueDeserializer unprivileged_deserializer(
        unprivileged_pairing_file);
    pairing =
        unprivileged_deserializer.Deserialize(&error_code, &error_message);
  }

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
  base::File::Error error;
  if (!base::CreateDirectoryAndGetError(registry_path_, &error)) {
    LOG(ERROR) << "Could not create pairing registry directory: " << error;
    return false;
  }

  base::DictValue pairing_value = pairing.ToValue();
  std::optional<std::string> pairing_json = base::WriteJson(pairing_value);
  if (!pairing_json.has_value()) {
    LOG(ERROR) << "Failed to serialize pairing data for "
               << pairing.client_id();
    return false;
  }

  base::FilePath pairing_file =
      registry_path_.Append(pairing.client_id() + kPrivilegedSuffix);
  if (!base::ImportantFileWriter::WriteFileAtomically(pairing_file,
                                                      *pairing_json)) {
    LOG(ERROR) << "Could not save pairing data for " << pairing.client_id();
    return false;
  }

  // The Implementation of WriteFileAtomically guarantees that the file has the
  // permission 600 since it is renamed from a file created with mktemp(), but
  // we explicitly set the permission here since it isn't documented.
  if (!base::SetPosixFilePermissions(pairing_file, 0600)) {
    LOG(ERROR) << "Failed to set permissions on privileged pairing file";
  }

  if (use_unprivileged_file_) {
    pairing_value.Remove(PairingRegistry::kSharedSecretKey);
    std::optional<std::string> unprivileged_pairing_json =
        base::WriteJson(pairing_value);
    if (!unprivileged_pairing_json.has_value()) {
      LOG(ERROR) << "Failed to serialize unprivileged pairing data for "
                 << pairing.client_id();
      return false;
    }

    base::FilePath unprivileged_pairing_file =
        registry_path_.Append(pairing.client_id() + kUnprivilegedSuffix);
    if (!base::ImportantFileWriter::WriteFileAtomically(
            unprivileged_pairing_file, *unprivileged_pairing_json)) {
      LOG(ERROR) << "Could not save unprivileged pairing data for "
                 << pairing.client_id();
      return false;
    }

    if (!base::SetPosixFilePermissions(unprivileged_pairing_file, 0644)) {
      LOG(ERROR) << "Failed to set permissions on unprivileged pairing file";
    }
  }

  return true;
}

bool PairingRegistryDelegateLinux::Delete(const std::string& client_id) {
  base::FilePath pairing_file =
      registry_path_.Append(client_id + kPrivilegedSuffix);
  base::FilePath unprivileged_pairing_file =
      registry_path_.Append(client_id + kUnprivilegedSuffix);

  bool success = base::DeleteFile(pairing_file);
  success = base::DeleteFile(unprivileged_pairing_file) && success;
  return success;
}

// static
base::FilePath PairingRegistryDelegateLinux::GetDefaultRegistryPath() {
  base::FilePath config_dir = remoting::GetConfigDir();
  return config_dir.Append(kRegistryDirectory);
}

// static
bool PairingRegistryDelegateLinux::SetupMultiProcessPairingRegistry() {
  // The pairing directory is under the config directory, which is owned by
  // root, so we need to create the pairing directory and change its owner to
  // the network process user.
  base::FilePath pairing_dir = GetDefaultRegistryPath();

  // Create the directory if it doesn't exist.
  base::File::Error error;
  if (!base::CreateDirectoryAndGetError(pairing_dir, &error)) {
    LOG(ERROR) << "Failed to create pairing registry directory: "
               << base::File::ErrorToString(error);
    return false;
  }

  // Set the owner to the network process user.
  auto user_info = GetPasswdUserInfo(GetNetworkProcessUsername());
  if (!user_info.has_value()) {
    LOG(ERROR) << "Failed to get network process user info: "
               << user_info.error();
    return false;
  }

  if (HANDLE_EINTR(chown(pairing_dir.value().c_str(), user_info->uid,
                         user_info->gid)) != 0) {
    PLOG(ERROR) << "Failed to chown pairing registry directory to "
                << GetNetworkProcessUsername();
    return false;
  }

  // Set permissions to 755 to allow any users to read the unprivileged pairing
  // files.
  if (!base::SetPosixFilePermissions(pairing_dir, 0755)) {
    LOG(ERROR) << "Failed to set permissions on pairing registry directory";
    return false;
  }
  return true;
}

std::unique_ptr<PairingRegistry::Delegate> CreatePairingRegistryDelegate() {
  return std::make_unique<PairingRegistryDelegateLinux>();
}

}  // namespace remoting
