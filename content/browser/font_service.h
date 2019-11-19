// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FONT_SERVICE_H_
#define CONTENT_BROWSER_FONT_SERVICE_H_

#include "components/services/font/public/mojom/font_service.mojom.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {

// Connects |receiver| to the global in-process instance of the Font service.
CONTENT_EXPORT void ConnectToFontService(
    mojo::PendingReceiver<font_service::mojom::FontService> receiver);

}  // namespace content

#endif  // CONTENT_BROWSER_FONT_SERVICE_H_
