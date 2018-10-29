// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_SERVICE_WORKER_TEST_OBSERVER_H_
#define CONTENT_PUBLIC_TEST_SERVICE_WORKER_TEST_OBSERVER_H_

#include "base/callback_forward.h"

class GURL;
namespace content {

class ServiceWorkerContext;

// Stops the active service worker of the registration for the given |scope|,
// and calls |complete_callback_ui| callback on UI thread when done.
//
// Can be called from UI/IO thread.
void StopServiceWorkerForScope(ServiceWorkerContext* context,
                               const GURL& scope,
                               base::OnceClosure complete_callback_ui);

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_SERVICE_WORKER_TEST_OBSERVER_H_
