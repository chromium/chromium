// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CLIENT_UTILS_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CLIENT_UTILS_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/service_worker/service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_client.mojom.h"

class GURL;

namespace blink {
class StorageKey;
}  // namespace blink

namespace content {

struct GlobalRenderFrameHostId;
class ServiceWorkerClient;
class ServiceWorkerContextCore;
class ServiceWorkerVersion;

// TODO(crbug.com/40568315): Many of these functions can return a synchronous
// value instead of using a callback since ServiceWorkerContext now lives on the
// UI thread.

namespace service_worker_client_utils {

using NavigationCallback = base::OnceCallback<void(
    blink::ServiceWorkerStatusCode status,
    blink::mojom::ServiceWorkerClientInfoPtr client_info)>;
using ClientCallback = base::OnceCallback<void(
    blink::mojom::ServiceWorkerClientInfoPtr client_info)>;

// The type of an opened window.
enum class WindowType {
  NEW_TAB_WINDOW = 0,
  PAYMENT_HANDLER_WINDOW,
};

// Focuses the window client associated with |service_worker_client|. |callback|
// is called with the client information on completion.
void FocusWindowClient(ServiceWorkerClient* service_worker_client,
                       ClientCallback callback);

// Opens a new window and navigates it to `url`. `callback` is called with the
// window's client information on completion. If `type` is NEW_TAB_WINDOW, we
// will open a new app window, if there is an app installed that has `url` in
// its scope. What an "installed app" is depends on the embedder of content. In
// Chrome's case, it is an installed Progressive Web App. If there is no such
// app, we will open a new foreground tab instead.
void OpenWindow(const GURL& url,
                const GURL& script_url,
                const blink::StorageKey& key,
                int worker_id,
                int worker_process_id,
                const base::WeakPtr<ServiceWorkerContextCore>& context,
                WindowType type,
                NavigationCallback callback);

// Navigates the client specified by `rfh_id` to `url`. `callback` is called
// with the client information on completion.
void NavigateClient(const GURL& url,
                    const GURL& script_url,
                    const blink::StorageKey& key,
                    const GlobalRenderFrameHostId& rfh_id,
                    const base::WeakPtr<ServiceWorkerContextCore>& context,
                    NavigationCallback callback);

// Gets the client specified by |service_worker_client|. |callback| is called
// with the client information on completion.
void GetClient(ServiceWorkerClient* service_worker_client,
               ClientCallback callback);

// Collects clients matched with |options|. |callback| is called with the client
// information sorted in MRU order (most recently focused order) on completion.
void GetClients(const base::WeakPtr<ServiceWorkerVersion>& controller,
                blink::mojom::ServiceWorkerClientQueryOptionsPtr options,
                blink::mojom::ServiceWorkerHost::GetClientsCallback callback);

// Called after a navigation. Uses `rfh_id` to find the ServiceWorkerClient
// where the navigation occurred and calls `callback` with its info once
// ServiceWorkerClient::is_execution_ready() is true. May call the callback with
// OK status but nullptr if the service worker client is already destroyed, or
// call the callback with an error status on error.
//
// `key` is only used for a CHECK_EQ check to ensure we don't accidentally
// get a cross-origin ServiceWorkerClient when "--disable-web-security"
// is inactive. But for scenarios where "--disable-web-security" is
// specified/active `script_url` will ensure the correct key (as inferred from
// the script_url during registration) is used.
void DidNavigate(const base::WeakPtr<ServiceWorkerContextCore>& context,
                 const GURL& script_url,
                 const blink::StorageKey& key,
                 NavigationCallback callback,
                 GlobalRenderFrameHostId rfh_id);

}  // namespace service_worker_client_utils

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CLIENT_UTILS_H_
