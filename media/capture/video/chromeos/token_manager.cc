// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/capture/video/chromeos/token_manager.h"

#include <grp.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"

namespace {

gid_t GetArcCameraGid() {
  auto* group = getgrnam("arc-camera");
  return group != nullptr ? group->gr_gid : 0;
}

bool EnsureTokenDirectoryExists(const base::FilePath& token_path) {
  static const gid_t gid = GetArcCameraGid();
  if (gid == 0) {
    LOG(ERROR) << "Failed to query the GID of arc-camera";
    return false;
  }

  base::FilePath dir_name = token_path.DirName();
  if (!base::CreateDirectory(dir_name) ||
      !base::SetPosixFilePermissions(dir_name, 0770)) {
    LOG(ERROR) << "Failed to create token directory at "
               << token_path.AsUTF8Unsafe();
    return false;
  }

  if (chown(dir_name.AsUTF8Unsafe().c_str(), -1, gid) != 0) {
    LOG(ERROR) << "Failed to chown token directory to arc-camera";
    return false;
  }
  return true;
}

bool WriteTokenToFile(const base::FilePath& token_path,
                      const base::UnguessableToken& token) {
  if (!EnsureTokenDirectoryExists(token_path)) {
    LOG(ERROR) << "Failed to ensure token directory exists";
    return false;
  }
  base::File token_file(
      token_path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  if (!token_file.IsValid()) {
    LOG(ERROR) << "Failed to create token file at "
               << token_path.AsUTF8Unsafe();
    return false;
  }
  token_file.WriteAtCurrentPos(base::as_byte_span(token.ToString()));
  return true;
}

}  // namespace

namespace media {

constexpr char TokenManager::kServerTokenPath[];
constexpr char TokenManager::kTestClientTokenPath[];
constexpr std::array<cros::mojom::CameraClientType, 3>
    TokenManager::kTrustedClientTypes;

TokenManager::TokenManager() = default;
TokenManager::~TokenManager() = default;

bool TokenManager::GenerateServerToken() {
  server_token_ = base::UnguessableToken::Create();
  return WriteTokenToFile(base::FilePath(kServerTokenPath), server_token_);
}

bool TokenManager::GenerateTestClientToken() {
  return WriteTokenToFile(
      base::FilePath(kTestClientTokenPath),
      GetTokenForTrustedClient(cros::mojom::CameraClientType::TESTING));
}

base::UnguessableToken TokenManager::GetTokenForTrustedClient(
    cros::mojom::CameraClientType type) {
  base::AutoLock l(client_token_map_lock_);
  if (!base::Contains(kTrustedClientTypes, type)) {
    return base::UnguessableToken();
  }
  auto& token_set = client_token_map_[type];
  if (token_set.empty()) {
    token_set.insert(base::UnguessableToken::Create());
  }
  return *token_set.begin();
}

void TokenManager::RegisterPluginVmToken(const base::UnguessableToken& token) {
  base::AutoLock l(client_token_map_lock_);
  auto result =
      client_token_map_[cros::mojom::CameraClientType::PLUGINVM].insert(token);
  if (!result.second) {
    LOG(WARNING) << "The same token is already registered";
  }
}

void TokenManager::UnregisterPluginVmToken(
    const base::UnguessableToken& token) {
  base::AutoLock l(client_token_map_lock_);
  auto num_removed =
      client_token_map_[cros::mojom::CameraClientType::PLUGINVM].erase(token);
  if (num_removed != 1) {
    LOG(WARNING) << "The token wasn't registered previously";
  }
}

bool TokenManager::AuthenticateServer(const base::UnguessableToken& token) {
  DCHECK(!server_token_.is_empty());
  return server_token_ == token;
}

std::optional<cros::mojom::CameraClientType> TokenManager::AuthenticateClient(
    cros::mojom::CameraClientType type,
    const base::UnguessableToken& token) {
  base::AutoLock l(client_token_map_lock_);
  if (type == cros::mojom::CameraClientType::UNKNOWN) {
    for (const auto& [client_type, token_set] : client_token_map_) {
      if (token_set.find(token) != token_set.end()) {
        return client_type;
      }
    }
    return std::nullopt;
  }
  auto& token_set = client_token_map_[type];
  if (token_set.find(token) == token_set.end()) {
    return std::nullopt;
  }
  return type;
}

void TokenManager::AssignServerTokenForTesting(
    const base::UnguessableToken& token) {
  server_token_ = token;
}

void TokenManager::AssignClientTokenForTesting(
    cros::mojom::CameraClientType type,
    const base::UnguessableToken& token) {
  base::AutoLock l(client_token_map_lock_);
  client_token_map_[type].insert(token);
}

}  // namespace media
