# Extension Layer/Service Worker Interactions
An extension background is the context that an extension runs on. It allows
extensions to react to events or messages with specified instructions. Up until
Manifest V2, there were two types of extension background pages, persistent
background pages and non-persistent background pages. As part of Manifest V3, we
are migrating extensions from the persistent/non-persistent background pages to
service workers. Service worker is a web platform feature that forms the basis
of app-like capabilities such as offline support, push notifications, and
background sync. A service worker is an event-driven JavaScript program that
runs in a worker thread. For a more detailed
explanation, see the [Service workers documentation](https://chromium.googlesource.com/chromium/src/+/HEAD/content/browser/service_worker/README.md).

This document describes the assumptions the //extensions layer makes when
relying on the service worker layer for registering/unregistering/starting a
service worker or ensuring the service worker’s liveness. It also documents
how ES modules can be imported in Manifest V3. Furthermore, it documents error
reporting and handling for service worker based extensions.

## Registration
When adding/loading an extension, `ExtensionRegistrar::ActivateExtension` is
called which results in calling `ServiceWorkerTaskQueue::ActivateExtension`
which calls `ServiceWorkerContext::RegisterServiceWorker`. During registration,
[`script_url`](https://source.chromium.org/chromium/chromium/src/+/77dcc35a2a0b98d3913148149496b8dd0d3464cc:content/public/browser/service_worker_context.h;l=125) is set to the URL corresponding to the relative path from
manifest.json's "background.service_worker" and scope is set to the extension
root, i.e., chrome-extension://<extension_id>/.

When registering the service worker, the //extensions layer relies on the
content layer’s guarantee that the registration is completed.
`OnRegistrationStored` is the first observer function that can guarantee
`StartWorkerForScope` can find the registration. After
`ServiceWorkerContextObserver::OnRegistrationStored`,
`ServiceWorkerContext::StartWorkerForScope` should be able to find the
registration.

## Unregistration
When an extension is removed/disabled/terminated,
`ExtensionRegistrar::DeactivateExtension` is called which will call
`ServiceWorkerTaskQueue::DeactivateExtension`. This will result in unregistering
the service worker, by calling `ServiceWorkerContext::UnregisterServiceWorker`.

## Registration/Unregistration failure
`DidRegisterServiceWorker` might fail, due to a few reasons: bad disk state,
invalid service worker script. The recovery steps would depend on the use case.

`DidUnregisterServiceWorker` failure is rare because it does not involve any
user-provided JS code.

When `DidRegisterServiceWorker/DidUnregisterServiceWorker` fails due to a disk
error, the SW layer will try to wipe the whole SW database as the current
implementation considers it a critical error.

## Starting the service worker
A service worker is started when a pending task (e.g. an event dispatch) is run.
A pending task is run only when all of the following conditions are met:
- Service worker registration has completed.
- Call to `ServiceWorkerContext::StartWorkerForScope` has returned.
- Worker thread (in the renderer) has seen
`DidStartServiceWorkerContextOnWorkerThread`.

ServiceWorkerTaskQueue starts a service worker via `StartWorkerForScope`.
We note that, in the current code, `StartWorkerForScope` should be called every
time before asking the worker to do something instead of relying on
`ServiceWorkerContextObserver::OnVersionStoppedRunning`.
The reason is that `OnVersionStoppedRunning` is called after the worker thread
is actually terminated. As a result, we can not rely on `OnVersionStoppedRunning`
to determine worker liveness. There are more fine-grained running status in the
content layer: RUNNING, STOPPING and STOPPED. The listener is called when the
worker’s state becomes STOPPED. When an event is dispatched to the worker, it
should not be done when the worker is in STOPPPING state.

The flow of dispatching an event to a service worker is

1- Calling `StartWorkerForScope` regardless of its running status,

2- Dispatching an event to a service worker inside of the callback triggered
from `StartWorkerForScope` synchronously.

In this way, we do not have to track whether the worker is running or not.

There are several possible reasons for `StartWorkerForScope` failure, such as
process allocation failure, timeout of the script evaluation, and disk
corruption.


## Notifications
When starting a service worker, the //extensions layer wait for readiness
notification from both the browser process and the renderer process. In the
current code, after receiving both notifications and before
`OnVersionStoppedRunning`, the //extensions layer assume that the SW is alive
and can dispatch events to an extension service worker. As explained above, we
should call `StartWorkerForScope` every time before asking the worker to do
something instead of relying on `OnVersionStoppedRunning`. We plan to fix this
in our code. Bug [1162193](https://bugs.chromium.org/p/chromium/issues/detail?id=1162193) tracks this fix.

## Service worker’s liveness
The //extensions layer rely on the service worker layer to ensure the service
worker’s liveness. We use EventAck IPC to ensure
that the service worker is alive while an event is dispatched. This is performed
in two steps:

1- An event is dispatched from the browser process to the renderer.

2- Renderer responds with EventAck to the browser process.

We ensure that between step 1 and step 2, we do not consider the service worker
as "inactive". We achieve this with workers, i.e., we call
`ServiceWorkerContext::StartingExternalRequest` on step 1, and then we call
`ServiceWorkerContext::FinishedExternalRequest` after step 2.

It is guaranteed that the worker will not be stopped between step 1 and step 2,
as long as we use `ServiceWorkerContext::StartingExternalRequest` and
`ServiceWorkerContext::FinishedExternalRequest`. The external request is a
mechanism to keep the worker alive.

## ES modules
[ES modules in service workers](https://chromium.googlesource.com/chromium/src/+/HEAD/content/browser/service_worker/es_modules.md) details the current state of ES module support
in service workers. In Manifest V3, type is added to the background key in the
manifest file, where two types are supported: classic and module. Type is set
to classic by default. For a module service worker, background.type should be
set to module in the manifest file. The following shows an example background
key in Manifest V3:
```
"background": {
  "service_worker": "sw.js",
  "type": "classic" (default) | "module"
}
```

## Error reporting and handling
For service worker-based extensions, when service worker registration fails, an
error is displayed in chrome://extensions page. Runtime errors are also
displayed in chrome://extensions page. When registering or starting a service
worker fails, a detailed error code is propagated from the content layer to the
//extensions layer, which enables the //extensions layer to display the error in
chrome://extensions to give developers a hint about the issue.
