// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/resource_coordinator_service.h"

#include "base/no_destructor.h"
#include "content/public/browser/browser_thread.h"
#include "services/resource_coordinator/public/mojom/resource_coordinator_service.mojom.h"
#include "services/resource_coordinator/resource_coordinator_service.h"

namespace content {

resource_coordinator::mojom::ResourceCoordinatorService*
GetResourceCoordinatorService() {
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::UI));
  static base::NoDestructor<
      mojo::Remote<resource_coordinator::mojom::ResourceCoordinatorService>>
      remote;
  static base::NoDestructor<resource_coordinator::ResourceCoordinatorService>
      service(remote->BindNewPipeAndPassReceiver());
  return remote->get();
}

memory_instrumentation::mojom::CoordinatorController*
GetMemoryInstrumentationCoordinatorController() {
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::UI));
  static base::NoDestructor<
      mojo::Remote<memory_instrumentation::mojom::CoordinatorController>>
      controller([] {
        mojo::Remote<memory_instrumentation::mojom::CoordinatorController> c;
        GetResourceCoordinatorService()
            ->BindMemoryInstrumentationCoordinatorController(
                c.BindNewPipeAndPassReceiver());
        return c;
      }());
  return controller->get();
}

}  // namespace content
