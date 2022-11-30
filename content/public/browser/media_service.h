// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_MEDIA_SERVICE_H_
#define CONTENT_PUBLIC_BROWSER_MEDIA_SERVICE_H_

#include "content/common/content_export.h"
#include "media/mojo/mojom/media_service.mojom-forward.h"

namespace content {

// Returns the browser's remote interface to the default global Media Service
// instance, which is started lazily and may run in- or out-of-process.
CONTENT_EXPORT media::mojom::MediaService& GetMediaService();

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_MEDIA_SERVICE_H_
