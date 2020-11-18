// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_TOKEN_MANAGER_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_TOKEN_MANAGER_H_

#include "base/containers/flat_map.h"
#include "base/unguessable_token.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom.h"

namespace media {

class TokenManager {
 public:
  TokenManager();
  ~TokenManager();

  bool GenerateServerToken();

  bool GenerateTestClientToken();

  base::UnguessableToken GetTokenForTrustedClient(
      cros::mojom::CameraClientType type);

  bool AuthenticateClient(cros::mojom::CameraClientType type,
                          const base::UnguessableToken& token);

 private:
  base::UnguessableToken server_token_;
  base::flat_map<cros::mojom::CameraClientType, base::UnguessableToken>
      client_token_map_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_TOKEN_MANAGER_H_
