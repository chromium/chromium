// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/frame_permission_controller.h"

#include "base/check_op.h"
#include "components/permissions/permission_util.h"
#include "content/public/browser/web_contents.h"
#include "url/origin.h"

using PermissionStatus = blink::mojom::PermissionStatus;
using PermissionType = blink::PermissionType;

namespace {

size_t GetPermissionIndex(PermissionType type) {
  size_t index = static_cast<size_t>(type);
  DCHECK_LT(index, static_cast<size_t>(PermissionType::NUM));
  return index;
}

constexpr PermissionStatus kDefaultPerOriginStatus = PermissionStatus::ASK;

// Converts from |url|'s actual origin to the "canonical origin" that should
// be used for the purpose of requesting permissions.
const url::Origin& GetCanonicalOrigin(PermissionType permission,
                                      const url::Origin& requesting_origin,
                                      const url::Origin& embedding_origin) {
  // Logic in this function should match the logic in
  // permissions::PermissionManager::GetCanonicalOrigin(). Currently it always
  // returns embedding origin, which is correct for all permissions supported by
  // WebEngine (AUDIO_CAPTURE, VIDEO_CAPTURE, PROTECTED_MEDIA_IDENTIFIER,
  // DURABLE_STORAGE).
  //
  // TODO(crbug.com/40680523): Update this function when other permissions are
  // added.
  return embedding_origin;
}

}  // namespace

FramePermissionController::PermissionSet::PermissionSet(
    PermissionStatus initial_state) {
  for (auto& permission : permission_states) {
    permission = initial_state;
  }
}

FramePermissionController::PermissionSet::PermissionSet(
    const PermissionSet& other) = default;

FramePermissionController::PermissionSet&
FramePermissionController::PermissionSet::operator=(
    const PermissionSet& other) = default;

FramePermissionController::FramePermissionController(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

FramePermissionController::~FramePermissionController() = default;

void FramePermissionController::SetPermissionState(PermissionType permission,
                                                   const url::Origin& origin,
                                                   PermissionStatus state) {
  // Currently only the following permissions are supported by WebEngine. Others
  // may not be handled correctly by this class.
  //
  // TODO(crbug.com/40680523): This check is necessary mainly because
  // GetCanonicalOrigin() may not work correctly for other permission. See
  // comemnts in GetCanonicalOrigin(). Remove it once that issue is resolved.
  DCHECK(permission == PermissionType::AUDIO_CAPTURE ||
         permission == PermissionType::VIDEO_CAPTURE ||
         permission == PermissionType::PROTECTED_MEDIA_IDENTIFIER ||
         permission == PermissionType::DURABLE_STORAGE);

  auto it = per_origin_permissions_.find(origin);
  if (it == per_origin_permissions_.end()) {
    // Don't create a PermissionSet for |origin| if |state| is set to the
    // per-origin default, since that would have no effect.
    if (state == kDefaultPerOriginStatus)
      return;

    it = per_origin_permissions_
             .insert(
                 std::make_pair(origin, PermissionSet(kDefaultPerOriginStatus)))
             .first;
  }

  it->second.permission_states[GetPermissionIndex(permission)] = state;
}

void FramePermissionController::SetDefaultPermissionState(
    PermissionType permission,
    PermissionStatus state) {
  DCHECK(state != PermissionStatus::ASK);
  default_permissions_.permission_states[GetPermissionIndex(permission)] =
      state;
}

PermissionStatus FramePermissionController::GetPermissionState(
    PermissionType permission,
    const url::Origin& requesting_origin) {
  url::Origin embedding_origin = url::Origin::Create(
      permissions::PermissionUtil::GetLastCommittedOriginAsURL(
          web_contents_->GetPrimaryMainFrame()));
  const url::Origin& canonical_origin =
      GetCanonicalOrigin(permission, requesting_origin, embedding_origin);

  PermissionSet effective = GetEffectivePermissionsForOrigin(canonical_origin);
  return effective.permission_states[GetPermissionIndex(permission)];
}

void FramePermissionController::RequestPermissions(
    const std::vector<PermissionType>& permissions,
    const url::Origin& requesting_origin,
    base::OnceCallback<void(const std::vector<PermissionStatus>&)> callback) {
  std::vector<PermissionStatus> result;
  result.reserve(permissions.size());

  for (auto& permission : permissions) {
    result.push_back(GetPermissionState(permission, requesting_origin));
  }

  std::move(callback).Run(result);
}

FramePermissionController::PermissionSet
FramePermissionController::GetEffectivePermissionsForOrigin(
    const url::Origin& origin) {
  PermissionSet result = default_permissions_;
  auto it = per_origin_permissions_.find(origin);
  if (it != per_origin_permissions_.end()) {
    // Apply per-origin GRANTED and DENIED states. Permissions with the ASK
    // state defer to the defaults.
    for (size_t i = 0; i < it->second.permission_states.size(); ++i) {
      if (it->second.permission_states[i] != kDefaultPerOriginStatus)
        result.permission_states[i] = it->second.permission_states[i];
    }
  }
  return result;
}
