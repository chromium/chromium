# DOM Storage

DOM Storage includes Local Storage and Session Storage. Code in `//content`
connects renderers to the Storage Service, where the data is managed. The
Storage Service implementations are in
[`//components/services/storage/dom_storage`][storage-service-dom-storage].

Renderers access both types of storage through
[`blink::mojom::StorageArea`][storage-area]. A storage area contains the
key-value data for a [`StorageKey`][storage-key], which identifies the context
whose data is being accessed. A Local Storage area is identified by its
`StorageKey`. A Session Storage area is identified by both its `StorageKey` and
a namespace ID.

[`DOMStorageContextWrapper`][dom-storage-context-wrapper] owns the
`//content` connections used to manage Local Storage and Session Storage.

## Local Storage

Local Storage does not use namespace IDs. A renderer requests an area for a
`StorageKey`, and [`storage::LocalStorageImpl`][local-storage-impl] manages the
areas for a storage partition.

## Session Storage namespaces

A namespace separates the Session Storage used by one tab or window from
another. The Session Storage namespace types have different responsibilities:

* [`content::SessionStorageNamespaceHandle`][content-session-storage-namespace-handle]
  is the `//content` interface for a namespace. It exposes the namespace ID and
  lets callers mark whether the data should remain available for session
  restore after the handle is destroyed.
* [`content::SessionStorageNamespaceHandleImpl`][content-session-storage-namespace-handle-impl]
  implements that interface. It uses
  [`storage::mojom::SessionStorageControl`][session-storage-control] to create
  and delete namespaces and to identify the source and destination of a clone.
* [`blink::mojom::SessionStorageNamespace`][blink-session-storage-namespace]
  lets a renderer order the clone with its storage area changes.
* [`storage::SessionStorageNamespaceImpl`][storage-session-storage-namespace-impl]
  implements that interface in the Storage Service. It owns a
  [`SessionStorageAreaImpl`][session-storage-area-impl] for each `StorageKey`,
  creates or loads areas as needed, binds requests to them, and coordinates
  cloning.

## Lifetime and session restore

[`NavigationController`][navigation-controller] keeps a namespace handle for
each storage partition it uses. Creating a new namespace sends
`SessionStorageControl::CreateNamespace()` to the Storage Service. Destroying
the last handle sends `SessionStorageControl::DeleteNamespace()`.

Chrome's session service saves the namespace ID with the tab and calls
`SetShouldPersist(true)`. Session restore recreates tabs after the browser
restarts. During session restore,
[`DOMStorageContext::RecreateSessionStorage()`][dom-storage-context] creates a
handle with the saved ID. The data can then be loaded from the Storage Service.

Tab restore allows a user to reopen a recently closed tab without restarting
the browser. It keeps a namespace handle in
[`sessions::ContentPlatformSpecificTabData`][content-platform-specific-tab-data]
so the restored tab can reuse the same namespace.

After startup, the Storage Service removes Session Storage data saved on disk
for namespaces that are no longer in use.

## Renderer access

Code in `//content` sends the namespace ID when it initializes the
renderer-side [`blink::WebView`][web-view]. Blink stores the ID in
[`blink::StorageNamespace`][storage-namespace] and uses it when requesting the
namespace and its storage areas.

Before forwarding a storage area request from a renderer,
`DOMStorageContextWrapper` verifies that the renderer process can access the
requested origin and that the identified frame may access the requested
`StorageKey`. The Storage Service then connects the request to the area within
the given namespace.

## Cloning

When `window.open()` creates a new window without `noopener`, the new window
receives a copy of the opening page's Session Storage. Blink allocates a
namespace ID for the new window and sends
`blink::mojom::SessionStorageNamespace::Clone()` on the original namespace.
Blink orders the clone with storage-area changes so that changes sent before
the clone are included and changes sent afterward are not. With `noopener`, no
copy is made.

Code in `//content` creates a handle for the new namespace and calls
`SessionStorageControl::CloneNamespace()` with the source namespace ID, the
destination namespace ID, and either `kWaitForCloneOnNamespace` or
`kImmediate`.

For a namespace that is in use by a renderer, `//content` uses
`kWaitForCloneOnNamespace`. The browser request and the renderer's ordered
`Clone()` request use different Mojo connections and can arrive in either
order. The Storage Service handles both orders, waiting for the renderer
request when the browser request arrives first.

A renderer request is not always needed. If the source namespace is not in use
but has saved data, the Storage Service can clone that data directly. Copies
initiated entirely in `//content`, such as copying a `NavigationController`,
use `kImmediate` and also proceed without a renderer `Clone()` request.

[blink-session-storage-namespace]: ../../../third_party/blink/public/mojom/dom_storage/session_storage_namespace.mojom
[content-platform-specific-tab-data]: ../../../components/sessions/content/content_platform_specific_tab_data.h
[content-session-storage-namespace-handle]: ../../public/browser/session_storage_namespace_handle.h
[content-session-storage-namespace-handle-impl]: session_storage_namespace_handle_impl.h
[dom-storage-context]: ../../public/browser/dom_storage_context.h
[dom-storage-context-wrapper]: dom_storage_context_wrapper.h
[local-storage-impl]: ../../../components/services/storage/dom_storage/local_storage_impl.h
[navigation-controller]: ../../public/browser/navigation_controller.h
[session-storage-control]: ../../../components/services/storage/public/mojom/session_storage_control.mojom
[session-storage-area-impl]: ../../../components/services/storage/dom_storage/session_storage_area_impl.h
[storage-area]: ../../../third_party/blink/public/mojom/dom_storage/storage_area.mojom
[storage-key]: ../../../third_party/blink/public/common/storage_key/storage_key.h
[storage-namespace]: ../../../third_party/blink/renderer/modules/storage/storage_namespace.h
[storage-service-dom-storage]: ../../../components/services/storage/dom_storage/
[storage-session-storage-namespace-impl]: ../../../components/services/storage/dom_storage/session_storage_namespace_impl.h
[web-view]: ../../../third_party/blink/public/web/web_view.h
