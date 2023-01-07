// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_MEDIA_SESSION_PUBLIC_CPP_UTIL_H_
#define SERVICES_MEDIA_SESSION_PUBLIC_CPP_UTIL_H_

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"

namespace media_session {

// Performs the playback |action| using |media_controller_ptr|.
COMPONENT_EXPORT(MEDIA_SESSION_CPP)
void PerformMediaSessionAction(
    mojom::MediaSessionAction action,
    const mojo::Remote<mojom::MediaController>& media_controller_remote);

}  // namespace media_session

#endif  // SERVICES_MEDIA_SESSION_PUBLIC_CPP_UTIL_H_
