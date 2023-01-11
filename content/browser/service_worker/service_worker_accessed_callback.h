// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_ACCESSED_CALLBACK_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_ACCESSED_CALLBACK_H_

#include "base/functional/callback.h"
#include "content/public/browser/allow_service_worker_result.h"
#include "url/gurl.h"

namespace content {

// This callback is used by a few places in the code but these places don't
// share a header which they all include, so this definition has to placed
// in a dedicated header.
using ServiceWorkerAccessedCallback =
    base::RepeatingCallback<void(const GURL&, AllowServiceWorkerResult)>;

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_ACCESSED_CALLBACK_H_
