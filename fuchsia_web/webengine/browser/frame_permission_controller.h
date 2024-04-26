// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_BROWSER_FRAME_PERMISSION_CONTROLLER_H_
#define FUCHSIA_WEB_WEBENGINE_BROWSER_FRAME_PERMISSION_CONTROLLER_H_

#include <array>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

namespace url {
class Origin;
}  // namespace url

namespace content {
class WebContents;
}  // namespace content

// FramePermissionController is responsible for web permissions state for a
// fuchsia.web.Frame instance.
class FramePermissionController {
 public:
  // |web_contents| must outlive FramePermissionController.
  explicit FramePermissionController(content::WebContents* web_contents);
  ~FramePermissionController();

  FramePermissionController(FramePermissionController&) = delete;
  FramePermissionController& operator=(FramePermissionController&) = delete;

  // Sets the |state| for the specified |permission| and |origin|.
  void SetPermissionState(blink::PermissionType permission,
                          const url::Origin& origin,
                          blink::mojom::PermissionStatus state);

  // Sets the default |state| for the specified |permission|. Setting |state| to
  // ASK causes the |default_permissions_| state to be used for |permission| for
  // this origin.
  // TODO(crbug.com/40680523): Allow ASK to be the default state, to indicate
  // that the user should be prompted.
  void SetDefaultPermissionState(blink::PermissionType permission,
                                 blink::mojom::PermissionStatus state);

  // Returns current permission state of the specified |permission| and
  // |requesting_origin|.
  blink::mojom::PermissionStatus GetPermissionState(
      blink::PermissionType permission,
      const url::Origin& requesting_origin);

  // Requests permission state for the specified |permissions|. When the request
  // is resolved, the |callback| is called with a list of status values, one for
  // each value in |permissions|, in the same order.
  //
  // TODO(crbug.com/40680523): Current implementation doesn't actually prompt
  // the user: all permissions in the ASK state are denied silently. Define
  // fuchsia.web.PermissionManager protocol and use it to request permissions.
  void RequestPermissions(
      const std::vector<blink::PermissionType>& permissions,
      const url::Origin& requesting_origin,
      base::OnceCallback<
          void(const std::vector<blink::mojom::PermissionStatus>&)> callback);

 private:
  struct PermissionSet {
    // Initializes all permissions with |initial_state|.
    explicit PermissionSet(blink::mojom::PermissionStatus initial_state);

    PermissionSet(const PermissionSet& other);
    PermissionSet& operator=(const PermissionSet& other);

    std::array<blink::mojom::PermissionStatus,
               static_cast<int>(blink::PermissionType::NUM)>
        permission_states;
  };

  // Returns the effective PermissionStatus for |origin|. If the per-|origin|
  // state is ASK, or there are no specific permissions set for |origin|, then
  // the default permission status takes effect. This means that it is not
  // currently possible to set a default of GRANTED/DENIED, and to override that
  // to ASK for specific origins.
  PermissionSet GetEffectivePermissionsForOrigin(const url::Origin& origin);

  content::WebContents* const web_contents_;

  base::flat_map<url::Origin, PermissionSet> per_origin_permissions_;
  PermissionSet default_permissions_{blink::mojom::PermissionStatus::DENIED};
};

#endif  // FUCHSIA_WEB_WEBENGINE_BROWSER_FRAME_PERMISSION_CONTROLLER_H_
