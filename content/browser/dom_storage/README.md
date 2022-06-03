# DOM Storage

*Under Contruction*

# Session Storage (Mojo)

*Under Contruction*

The browser manages the lifetime of session storage namespaces with
[SessionStorageNamespaceImpl](
https://cs.chromium.org/chromium/src/content/browser/dom_storage/session_storage_namespace_impl.h).

The object calls [`SessionStorageContextMojo::CreateSessionNamespace`](
https://cs.chromium.org/chromium/src/content/browser/dom_storage/session_storage_context_mojo.h?dr=CSs&l=50)
when it is created, and [`SessionStorageContextMojo::DeleteSessionNamespace`](
https://cs.chromium.org/chromium/src/content/browser/dom_storage/session_storage_context_mojo.h?dr=CSs&l=53)
when it's destructed.


This object is primarily held by both the [`NavigationControllerImpl`](
https://cs.chromium.org/chromium/src/content/browser/renderer_host/navigation_controller_impl.h?dr=CSs&l=426)
and in the [`ContentPlatformSpecificTabData`](
https://cs.chromium.org/chromium/src/components/sessions/content/content_platform_specific_tab_data.h?dr=C&l=35)
which is used to restore tabs. The services stores recent tab
closures for possible browser restore [here](
https://cs.chromium.org/chromium/src/components/sessions/core/tab_restore_service_helper.h?dr=C&l=186).

In the future when it's fully mojo-ified, the lifetime will be managed by the
mojo [`SessionStorageNamespace`](
https://cs.chromium.org/chromium/src/content/common/storage_partition_service.mojom)
which can be passed to the renderer and the session restore service. There will
always need to be an ID though as we save this ID to disk in the session
restore service.

## High Level Access And Lifetime Flow

 1. Before a renderer tab is opened, a `SessionStorageNamespaceImpl` object is
    created, which in turn calls
    `SessionStorageContextMojo::CreateSessionNamespace`.
    * This can happen by either navigation or session restore.
 1. The following can happen in any order:
    * Renderer creation, usage, and destruction
       * The `session_namespace_id` is sent to the renderer, which uses
         `StorageParitionService` to access storage.
       * The renderer is destroyed, calling
         `SessionStorageContextMojo::DeleteSessionNamespace`.
          * If `SetShouldPersist(true)` was not called (or called with false),
            then the data is deleted from disk.
    * `SetShouldPersist(true)` is called from the session restores service,
      which means the data should NOT be deleted on disk when the namespace is
      destroyed. This is called for all tabs that the session restore services
      wants to persist to disk.
    * The session restore service calls
      `DomStorageContext::StartScavengingUnusedSessionStorage` to clean up any
      namespaces that are on disk but were not used by any recreated tab. This
      is an 'after startup task', and usually happens before `SetShouldPesist`
      is called.

## Possible Edge Case: Persisted Data vs Stale Disk Data

Namespace is created, persisted, destroyed, and then we scavange unused session
storage.

Flow:
 1. `SessionStorageContextMojo::CreateSessionNamespace`
 1. `SetShouldPersist(true)`
 1. `SessionStorageContextMojo::DeleteSessionNamespace`
 1. `DomStorageContext::StartScavengingUnusedSessionStorage`
 1. The data should still reside on disk after scavenging.

The namespace could accidentally be considered a 'leftover' namespace by the
scavenging algorithm and deleted from disk.

## Navigation Details

When navigating from a previous frame, the previous frame will allocate a new
session storage id for the new frame, as well as issue the 'clone' call [here](https://cs.chromium.org/chromium/src/content/renderer/render_view_impl.cc?q=RenderViewImpl::RenderViewImpl&l=1273).

The `session_namespace_id` for a frame's session storage is stored in the
`CreateNewWindowParams` object in [frame.mojom](https://cs.chromium.org/chromium/src/content/common/frame.mojom).

If the frame wasn't created from a previous frame, the SessionStorage namespace
object is created [here](https://cs.chromium.org/chromium/src/content/browser/renderer_host/navigation_controller_impl.cc?type=cs&l=1904)
and the id is accessed [here](https://cs.chromium.org/chromium/src/content/browser/renderer_host/render_view_host_impl.cc?type=cs&l=321).

## Renderer Connection to Session Storage

Renderers use the `session_namespace_id` from the `CreateNewWindowParams`. They
access session storage by using [`StoragePartitionService::OpenSessionStorage`](
https://cs.chromium.org/chromium/src/content/common/storage_partition_service.mojom),
and then `SessionStorageNamespace::OpenArea` with the `session_namespace_id`.

They can then bind to a `LevelDBWrapper` on a per-origin basis.

## Session Restore Service Interaction

A reference to the session is stored in the [`ContentPlatformSpecificTabData`](
https://cs.chromium.org/chromium/src/components/sessions/content/content_platform_specific_tab_data.h?dr=C&l=35)
which is used to restore recently closed tabs. The services stores recent tab
closures for possible browser restore [here](
https://cs.chromium.org/chromium/src/components/sessions/core/tab_restore_service_helper.h?dr=C&l=186).

When tabs are inserted, the session storage service saves the id to disk [here](https://cs.chromium.org/chromium/src/chrome/browser/sessions/session_service.cc?type=cs&l=313)
using the `commands` (which are saved to disk). The session id is also accessed
here for saving in `commands` in `TabClosing` and `BuildCommandsForTab`.
