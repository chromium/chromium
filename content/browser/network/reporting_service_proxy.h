// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NETWORK_REPORTING_SERVICE_PROXY_H_
#define CONTENT_BROWSER_NETWORK_REPORTING_SERVICE_PROXY_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/reporting/reporting.mojom.h"

namespace content {

class DedicatedWorkerHost;
class RenderFrameHost;
class ServiceWorkerHost;
class SharedWorkerHost;

// These methods method bind a mojom::ReportingServiceProxy for the specified
// object type. They must be called on the UI thread.
void CreateReportingServiceProxyForFrame(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::ReportingServiceProxy> receiver);
void CreateReportingServiceProxyForServiceWorker(
    ServiceWorkerHost* service_worker_host,
    mojo::PendingReceiver<blink::mojom::ReportingServiceProxy> receiver);
void CreateReportingServiceProxyForSharedWorker(
    SharedWorkerHost* shared_worker_host,
    mojo::PendingReceiver<blink::mojom::ReportingServiceProxy> receiver);
void CreateReportingServiceProxyForDedicatedWorker(
    DedicatedWorkerHost* dedicated_worker_host,
    mojo::PendingReceiver<blink::mojom::ReportingServiceProxy> receiver);

}  // namespace content

#endif  // CONTENT_BROWSER_NETWORK_REPORTING_SERVICE_PROXY_H_
