// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MEDIA_MANIFEST_H_
#define MEDIA_MOJO_SERVICES_MEDIA_MANIFEST_H_

#include "services/service_manager/public/cpp/manifest.h"

namespace media {

const service_manager::Manifest& GetMediaManifest();
const service_manager::Manifest& GetMediaRendererManifest();

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MEDIA_MANIFEST_H_
