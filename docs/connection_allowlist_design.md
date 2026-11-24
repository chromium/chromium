# Connection Allowlist Design

References:
* [Explainer][1]
* [Spec][2]
* [Chrome Status Entry][3]
* [Design Document][4]

## Summary

Connection Allowlist restricts network connections in Chromium using a
URLPattern allowlist returned by the server. The high-level flow is:

* The allowlist is returned in the HTTP response when loading a given
  context (document or worker)
* A `network_restrictions_id` is assigned to that context, which is used to
  identify requests that originate from it
* Enforcement then focuses on blocking requests containing that ID based on
  the mapped allowlist.

This document outlines the architectural design and the checks performed
across different Chromium layers (network service, browser, renderer).

All security and resource restrictions (CSP, Connection Allowlists, etc.) are
applied **restrictively and additively**. A request must satisfy **all**
applicable security policies to proceed; if any policy blocks the request, the
request is blocked.

## Design

### Network service integration

The network service is the primary layer where network requests are handled and
dispatched. The majority of Connection Allowlist enforcement occurs here.
* **Responsibilities:**
  * Intercept and filter outgoing network requests.
  * Apply allowlist rules at the URLLoader level (including document-initiated
    prefetches which are associated with the document's
    `network_restrictions_id`).
  * Enforce allowlists for other networking APIs like WebSockets, DNS Prefetch,
    and WebTransport.
* **Benefits:**
  * **Defense-in-Depth:** Since enforcement occurs in the Network Service
    process, compromised renderer processes cannot bypass these checks.
  * **Centralized Enforcement:** The Network Service handles HTTP/HTTPS
    requests, WebSockets, and WebTransport. Enforcing allowlists at this level
    provides a single, uniform point of control across different network
    protocols.
  * **Semantic Consistency:** Once registered, allowlist matching happens
    within the Network Service process. This ensures that the same set of
    rules is applied to all requests regardless of where they originated
    (e.g. fetch, fetch redirects, prefetch, worker, etc.).
  * **Extensibility:** The token-based mapping (`network_restrictions_id`) is
    highly extensible. Other future security features or JS-based network
    restriction APIs (similar to the dynamic
    `window.fence.disableUntrustedNetwork()` design previously used for Fenced
    Frames, see [CL 7709872][5]) can leverage this same plumbing to dynamically
    register and enforce rules in the Network Service.

#### Network restrictions ID (`network_restrictions_id`)

To map network requests back to the corresponding context (document or worker)
and its connection allowlists, each context is associated with a unique token,
the `network_restrictions_id` (a `base::UnguessableToken`).

* **Creation & Propagation**:
  * During cross-document navigation, a new `network_restrictions_id` is
    created on the `NavigationRequest` in
    `NetworkRestrictionsNavigationThrottle`.
  * Upon navigation commit, this ID is wrapped in a ref-counted handle
    (`base::RefCountedData<base::UnguessableToken>`) and stored in the
    document's `DocumentAssociatedData` (or worker hosts like
    `DedicatedWorkerHost` and `SharedWorkerHost`).
  * Ref-counting is used because there are edge cases where multiple documents
    can share the same `network_restrictions_id` (e.g., initial empty documents
    inheriting their creator's ID).
* **Updating the Network Service**:
  * If a document has active connection allowlists (enforced or report-only)
    determined from policy container policies, the browser process registers
    them via `StoragePartitionImpl::RestrictNetworkForIdsInNetworkContext`.
  * This sends the ID and allowlisted patterns to the `NetworkContext` in the
    Network Service using `RestrictNetworkForIds()`.
  * The navigation is deferred during the throttle check
    (`WillProcessResponse` / `WillCommitWithoutUrlLoader`) until the Network
    Service confirms the restrictions have been registered, ensuring
    restrictions are applied before resource loading begins.
* **Backup & Recovery**:
  * `StoragePartitionImpl` stores a copy of the restrictions in
    `network_restrictions_ids_`. In case of a Network Service crash, this map
    is used to restore the network restrictions on the new `NetworkContext`.
* **Lifetime & Cleanup**:
  * Every registered `network_restrictions_id` has a clear owner responsible for
    scheduling cleanup via
    `StoragePartitionImpl::ClearNetworkRestrictionsAfterDelay` (which only sends
    `NetworkContext::ClearNetworkRestrictions` IPCs for IDs actually present in
    `StoragePartitionImpl::network_restrictions_ids_`):
    * **Pre-commit navigations**: `NavigationRequest` tracks the
      `StoragePartitionImpl` where its `network_restrictions_id` was registered
      and clears it if the navigation fails (`OnRequestFailedInternal`, e.g.,
      before committing an error page) or is destroyed without committing.
    * **Committed documents**: `DocumentAssociatedData` clears its
      ref-counted `network_restrictions_id` when the last document holding a
      reference is destroyed (checking
      `StoragePartitionImpl::HasNetworkRestrictions` first so unrestricted
      documents do not queue delayed tasks).
    * **Workers**: `DedicatedWorkerHost`, `SharedWorkerHost`, and
      `ServiceWorkerVersion` clear their `network_restrictions_id` on
      destruction (`ServiceWorkerVersion` and `ServiceWorkerRegisterJob` check
      `StoragePartitionImpl::HasNetworkRestrictions` first so unrestricted
      workers do not queue delayed tasks). During Service Worker registration
      and update checks, `ServiceWorkerRegisterJob` clears
      `creator_network_restrictions_id_` in its destructor. Script update checks
      restrict script fetches via `creator_network_restrictions_id_` without
      pre-registering a candidate worker ID; only when an update (`kDifferent`)
      creates a new `ServiceWorkerVersion` does `StartWorker()` register that
      version's `network_restrictions_id` via
      `MaybeRegisterNetworkRestrictions()`.
  * **Selecting the ID at Calling Sites**:
    When a calling site initiates a request or creates a connection (e.g.
    WebSocket, WebTransport) that expects a `network_restrictions_id`, it
    should use:
    * **The context's ID**: If the request/connection is associated with a
      specific context (document, dedicated worker, shared worker, or service
      worker), use that context's `network_restrictions_id`.
    * **`network::GetNoOpNetworkRestrictionsId()`**: When no network
      restrictions should be applied, bypassing restrictions entirely. This
      should be used carefully as it bypasses the Connection Allowlist (e.g.,
      for browser-initiated requests that do not have an associated
      frame/context).
    * **`network::GetTestNetworkRestrictionsId()`**: In tests that require a
      valid network restrictions ID but do not test Connection Allowlist
      behavior.

### Browser process checks (content/browser)

The browser process coordinates navigation checks and handles security
policies.
* **Responsibilities:**
  * Store and manage Connection Allowlists as part of the `PolicyContainerHost`
    associated with each document.
  * Intercept and validate navigations in the browser process before they are
    sent to the network service using the
    `NavigationRequest::IsAllowedByConnectionAllowlist` function. This function
    enforces allowlist policies during key phases of navigation (such as
    initial request start and handling redirects).

### Blink checks

Blink is the rendering engine running in the renderer process.
* **Responsibilities:**
  * Enforce Connection Allowlists in the renderer process for:
    1. WebRTC connections. These checks are also validated in the network
       service to handle compromised renderers.
    2. Requests initiated via `fetch()` when they will be intercepted by a
       Service Worker. This is done in blink because the renderer directly
       talks to the Service Worker URLLoaderFactory in these cases, not giving
       the document's URLLoaderFactory checks a chance to run, unless the SW
       decides to fallback on the original fetch. This applies when the
       initiating context is either a document, dedicated worker or shared
       worker.

### Integration with service workers
Service Workers act as network proxies in the renderer process, intercepting
requests before they reach the network.
* **Responsibilities:**
  * **Main Script Fetches & Update Checks**: Subject Service Worker main script
    fetches and update checks to the Connection Allowlists of the
    **creator/initiator context**(the registering document).
    * **Initial Install**: The browser process creates the loader factory via
      `GetLoaderFactoryForMainScriptFetch` passing the
      `creator_network_restrictions_id_`.
    * **Update Checks**: During soft update checks (in
      `ServiceWorkerRegisterJob::UpdateAndContinue`), the loader factory is
      created via `GetLoaderFactoryForUpdateCheck` passing the
      `creator_network_restrictions_id_`.
  * **Intercepted Document Fetches**: Ensure requests initiated by a document
    that are intercepted by a Service Worker are validated in Blink against the
    **document's** Connection Allowlists before they are forwarded to the
    Service Worker. This prevents documents from bypassing their own
    restrictions, including when the Service Worker attempts to serve
    responses from Cache Storage.
  * **Service Worker Outgoing Fetches**: Apply the Service Worker's own
    Connection Allowlists (persisted and restored via the database) to its
    own fetches.
    * Requests initiated directly by the Service Worker context
      (`ServiceWorkerGlobalScope`) itself (via `fetch()`) are governed by the
      policies stored in its own `PolicyContainerHost`. Those are mapped to
      the Service Worker's own `network_restrictions_id` in the Network
      Service via the `NetworkRestrictionsWorkerThrottle`. This throttle also
      does the same for dedicated and shared worker contexts.
    * Once a request is inside the Service Worker, any outgoing network
      fetches (e.g., via `event.respondWith(fetch(request))`) are initiated
      using the Service Worker's `URLLoaderFactory` and subjected to the
      **Service Worker's** Connection Allowlists (associated with the Service
      Worker's own `network_restrictions_id`), regardless of the client
      document's policies. Consequently, redirects of these fetches are
      checked against the Service Worker's connection allowlist's redirect
      flag, not the document's.
  * **Navigation Preload**: Navigation preload requests are subject to the
    **Service Worker's** Connection-Allowlist. If the navigation itself is
    allowed by the document's Connection-Allowlist, the associated navigation
    preload request will be executed using the Service Worker's
    `URLLoaderFactory` (which is associated with the Service Worker's
    `network_restrictions_id`), thereby enforcing the Service Worker's
    policies.
  * **clients.navigate() in Service Workers**: Navigation initiated via
    clients.navigate() is subject to both the Service Worker's and the
    document's Connection Allowlists. Since the navigation is initiated from
    the Service Worker context, the Service Worker's Connection Allowlists
    apply to the navigation before it is initiated from the Service Worker.
    In addition, since the navigation is on a window client (which has an
    associated Document), the Document's Connection Allowlists also apply,
    and if there is a server redirect, the document's Connection Allowlists'
    redirect bit will be checked.
  * **clients.openWindow() in Service Workers**: Navigation initiated via
    clients.openWindow() is subject to the Service Worker's Connection
    Allowlists before it is initiated. There is no check on redirect since
    there is no CA to be checked once the navigation has started.

[1]: https://github.com/WICG/connection-allowlists
[2]: https://wicg.github.io/connection-allowlists/
[3]: https://chromestatus.com/feature/5175745573945344
[4]: https://docs.google.com/document/d/1B3LERUObjVDAKBNLpdIxbk8LC96rWUn1q8vtP9pPIuA/edit?usp=sharing
[5]: https://chromium-review.googlesource.com/c/chromium/src/+/7709872
