// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NET_REPORTING_SERVICE_PROXY_H_
#define CONTENT_BROWSER_NET_REPORTING_SERVICE_PROXY_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/reporting/reporting.mojom.h"

namespace content {

// Binds a mojom::ReportingServiceProxy to |request| that queues reports using
// |render_process_id|'s NetworkContext. This must be called on the UI thread.
void CreateReportingServiceProxy(
    int render_process_id,
    mojo::PendingReceiver<blink::mojom::ReportingServiceProxy> receiver);

}  // namespace content

#endif  // CONTENT_BROWSER_NET_REPORTING_SERVICE_PROXY_H_
