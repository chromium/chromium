# Service workers
[content/browser/service_worker]: /content/browser/service_worker
[content/renderer/service_worker]: /content/renderer/service_worker
[content/renderer/service_worker]: /content/renderer/service_worker
[disk_cache]: /net/disk_cache/README.md
[embedded_worker.mojom]: https://codesearch.chromium.org/chromium/src/third_party/blink/public/mojom/service_worker/embedded_worker.mojom
[service_worker_container.mojom]: https://codesearch.chromium.org/chromium/src/third_party/blink/public/mojom/service_worker/service_worker_container.mojom
[service_worker_database.h]: https://codesearch.chromium.org/chromium/src/components/services/storage/service_worker/service_worker_database.h
[third_party/blink/common/service_worker]: /third_party/blink/common/service_worker
[third_party/blink/public/common/service_worker]: /third_party/blink/public/common/service_worker
[third_party/blink/public/mojom/service_worker]: /third_party/blink/public/mojom/service_worker
[third_party/blink/public/platform/modules/service_worker]: /third_party/blink/public/platform/modules/service_worker
[third_party/blink/public/web/modules/service_worker]: /third_party/blink/public/web/modules/service_worker
[third_party/blink/renderer/modules/service_worker]: /third_party/blink/renderer/modules/service_worker
[Blink Public API]: /third_party/blink/public/README.md
[Cache Storage API]: /content/browser/cache_storage/README.md
[LevelDB]: /third_party/leveldatabase/README.chromium
[Onion Soup]: https://docs.google.com/document/d/1K1nO8G9dO9kNSmtVz2gJ2GG9gQOTgm65sJlV3Fga4jE/edit?usp=sharing
[Quota Manager]: /storage/browser/quota
[ServiceWorkerDatabase]: https://codesearch.chromium.org/chromium/src/components/services/storage/service_worker/service_worker_database.h
[ServiceWorkerStorage]: https://codesearch.chromium.org/chromium/src/components/services/storage/service_worker/service_worker_storage.h
[Service Worker specification]: https://w3c.github.io/ServiceWorker/
[MDN documentation]: https://developer.mozilla.org/en-US/docs/Web/API/Service_Worker_API

This document describes Chromium's implementation of [service
workers](https://developer.mozilla.org/en-US/docs/Web/API/Service_Worker_API).

[TOC]

## Introduction to service workers

This section briefly introduces what service workers are. For a more detailed
treatment, see the [MDN documentation] or the [Service Worker specification].

Service workers are a web platform feature that form the basis of app-like
capabilities such as offline support, push notifications, and background sync.
A service worker is a event-driven JavaScript program that runs in a worker
thread separate from a document.

Once registered, a service worker is installed on the browser and persists
indefinitely until evicted or deleted manually (see [Eviction](#Eviction)
below). The browser dispatches events to the worker thread, starting the
thread whenever needed and stopping it when there are no more events to
dispatch.

Service workers are bound to an origin. More specifically they have a *scope*
URL, specified when the service worker is registered. The service worker
*controls* pages or web workers that match its scope. There can be only one
service worker registration for a given scope.

A website registers a service worker using the `register()` API:
```javascript
navigator.serviceWorker.register('sw.js', {scope: './foo'});
```

If this page is on `https://example.com`, the service worker is registered for
scope `https://example.com/foo`.

The service worker may look like this:

```javascript
// sw.js:
self.addEventListener('install', event => {
  // Install static assets.
  event.waitUntil((async () => {
    const cache = await caches.open('my-cache');
    await cache.addAll(['all.css', 'page.js', 'page.html']);
  })());
});

self.addEventListener('fetch', event => {
  // Respond with a cached resource, or else fetch from network.
  event.respondWith((async () => {
    const response = await caches.match(event.request);
    return response || fetch(event.request);
  })());
});
```

Note the `fetch` event handler. A core functionality of service workers is the
ability to intercept and respond to URL requests, similar to a network proxy.
Whenever the browser makes a URL request that a service worker can intercept, it
dispatches a `fetch` event to the worker. The service worker can then provide a
response to the request, for example, by using the Fetch API, the Cache Storage
API, or by generating a response using `new Response()`.

To understand which service worker intercepts a URL request, there are two
rules.

1. *Main resource requests* are requests for a window or a web worker. When the
   browser makes a main resource request, it finds the service worker
   registration whose scope contains the URL, if any (example:
   `https://example.com/foo/hi` matches the service worker above). If so, that
   service worker intercepts the request, and the service worker subsequently
   controls the resulting window or web worker.
2. *Subresource requests* are requests from a window or a web worker, such as
   for stylesheets, scripts, or images. The service worker that controls the
   window or web worker intercepts these requests.

The rest of this document explains how service workers are implemented in
Chromium.

## Class overview

As a web platform feature, service worker is implemented in the [content
module](/content/README.md) and its dependency
[Blink](/third_party/blink/README.md).  Chrome-specific hooks and additions, for
example, for Chrome extensions support, are in higher-level directories like
[//chrome](/chrome/README.md).

The service worker implementation has parts in both the browser process and
renderer process:
- The browser process manages service worker registrations, initiates starting
  and stopping service worker threads in the renderer, requests the renderer
  to dispatch events to the workers, and implements most of the service worker
  APIs that the renderer process exposes to the web.
- The renderer process runs service worker threads, dispatches events to them,
  and provides the web-exposed API surface.

> TODO: A simple diagram of the browser/renderer architecture and the Mojo
> message pipes and interfaces would be helpful.

### Browser process

> Note: The classes in this section are in the namespace `content`.

In the browser process, `ServiceWorkerContextCore` is root class which all other
service worker classes are attached to. There is one context per [storage
partition](/content/public/browser/storage_partition.h).

`ServiceWorkerContextCore` is owned by a thread-safe refcounted wrapper called
`ServiceWorkerContextWrapper`. `StoragePartition` is the primary owner of this
object on the UI thread. But `ServiceWorkerContextCore` itself, and the classes
that hang off of it, are primarily single-threaded and run on the IO thread.
There is ongoing work to move this "service worker core" thread to the UI
thread. After that time, it may be possible to remove the refcounted wrapper and
have StoragePartition uniquely own the context core on the UI thread. See
the [Service Worker on UI design
doc](https://docs.google.com/document/d/1APPz704Ebcrwp0QEaPNLVtBjPuV6MXlolby7AtZB4MA/edit?usp=sharing).

The context owns `ServiceWorkerStorage`, which manages service worker
registrations and auxiliary data attached to them. The `ServiceWorkerStorage`
owns a `ServiceWorkerDatabase`, which provides access to the LevelDB instance
containing the registration data. See [Storage](#Storage) below.

`ServiceWorkerStorage` is used to register, update, and unregister service
workers. Typically these operations are driven by `ServiceWorkerRegisterJob` and
`ServiceWorkerUnregisterJob`, which implement the
[*jobs*](https://w3c.github.io/ServiceWorker/#dfn-job) defined in the
specification. As per the specification, the jobs are run sequentially using a a
[*job queue*](https://w3c.github.io/ServiceWorker/#dfn-job-queue).  The class
`ServiceWorkerJobCoordinator`, owned by the context, implements this queue.

`ServiceWorkerStorage` represents service worker entities as
`ServiceWorkerRegistration` and `ServiceWorkerVersion`. These correspond to the
specification's model of [*service worker
registration*](https://w3c.github.io/ServiceWorker/#dfn-service-worker-registration)
and [*service worker*](https://w3c.github.io/ServiceWorker/#dfn-service-worker),
respectively.

`ServiceWorkerVersion` provides functions for starting and stopping a service
worker thread in the renderer, and for dispatching events to the thread.
It uses a lower-level class, `EmbeddedWorkerInstance`,
to request the renderer to start and stop the service worker thread.

> Note: The "embedded worker" terminology and abstraction is a bit of a historical
> accident. At one point the plan was for service workers and shared workers to
> use the same "embedded worker" classes. But it turned out only service workers
> use it.

A running service worker has a corresponding host in the browser process called
`ServiceWorkerHost`.

In addition, service worker clients (windows and web workers) are represented by
a `ServiceWorkerContainerHost` in the browser process. This host holds
information pertinent to service workers, such as which
`ServiceWorkerRegistration` is controlling the client, and it implements the
Mojo interface the renderer uses for the [client-side service worker
API](https://w3c.github.io/ServiceWorker/#document-context).

### Renderer process

> Note: Historically much service worker code in the renderer process was
> implemented in `//content/renderer`. There is ongoing work to move it to
> `//third_party/blink` per [Onion Soup], which will remove some layers of
> indirection.

The renderer process naturally has classes that implement the web-exposed
interfaces: `blink::ServiceWorker`, `blink::ServiceWorkerRegistration`,
`blink::ServiceWorkerContainer`, etc.

Other classes in the renderer process can be divided into those that deal with
a) service worker execution contexts, and b) service worker clients (windows and
web workers).

#### Service worker execution contexts

For starting and stopping a service worker,
`content::EmbeddedWorkerInstanceClientImpl` is used. One is created per service
worker startup on a background thread. It creates a
`content::ServiceWorkerContextClient`, which owns a
`blink::WebEmbeddedWorkerImpl`, which creates a `blink::ServiceWorkerThread`
which starts the physical service worker thread and JavaScript execution context
with a `blink::ServiceWorkerGlobalScope` global.

`ServiceWorkerGlobalScope` implements two Mojo interfaces:
- `mojom.blink.ServiceWorker`, which the browser process uses to dispatch events
  to the service worker.
- `mojom.blink.ControllerServiceWorker`, which other renderer processes use to
  dispatch fetch events to a service worker that controls a client in that
  process.

#### Service worker clients

Service worker clients have an associated
`content::ServiceWorkerProviderContext` which contains information such as which
service worker controls the client and manages request interception to that
service worker.

### Mojo

[Mojo](/mojo/README.md) is Chromium's IPC system and plays a important role in
service worker architecture. This section describes the main Mojo interfaces for
service workers, and which message pipes they are on.

#### Browser <->  Renderer (window)

For windows (or [clients](https://w3c.github.io/ServiceWorker/#service-worker-client-concept)),
the browser process and renderer process talk over Mojo interfaces bound to the Mojo pipe to commit
a navigation, which is considered as the legacy IPC "channel" message pipe. This guarantees the
order of IPC messages between Mojo interfaces.

Each window in the renderer process is connected to a host in the browser
process. The renderer talks to the browser process over the
`mojom.blink.ServiceWorkerContainerHost` interface which provides functionality
like registering service workers. The browser talks to the renderer over the
`mojom.blink.ServiceWorkerContainer` interface.

The window obtains `ServiceWorkerRegistration` and `ServiceWorker` JavaScript
objects via APIs like `navigator.serviceWorker.ready`,
`navigator.serviceWorker.controller`, and `navigator.serviceWorker.register()`.
Each object has a connection to the browser, again on the channel-associated
message pipe. `ServiceWorkerRegistration` has a remote to a
`mojom.blink.ServiceWorkerRegistrationObjectHost` and `ServiceWorker` has a
remote to a `ServiceWorkerObjectHost`. Conversely, the browser process has
remotes to `mojom.blink.ServiceWorkerRegistrationObject` and
`mojom.blink.ServiceWorkerObject`.

> After making this design, there's been some realization that asynchronous
> ownership makes destruction complicated because of non-deterministic
> destruction order sometimes caused crashes.
> It may have worked better to use fewer interfaces, e.g., a single
> ServiceWorkerContainer interface from which one can manipulate
> ServiceWorkerRegistration and ServiceWorker, or maybe prohibiting destructions
> initiated from the renderer may work.
> In addition, we have a Mojo interface for in-process communication across threads like
> [this](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/public/mojom/service_worker/controller_service_worker.mojom;l=95;drc=6e8b402a6231405b753919029c9027404325ea00).
> Mojo is now slightly overused for abstraction of layers for service workers.

#### Browser <-> Renderer (shared worker)

For shared workers, the browser process and renderer process talk over Mojo
interfaces bound to the dedicated message pipe established by
the `mojom.blink.SharedWorkerFactory` implementation that creates the shared
worker.

Similar to windows, the shared worker has a remote to a
`mojom.blink.ServiceWorkerContainerHost` in the browser process, and the
browser process has a remote to the `mojom.blink.ServiceWorkerContainer`
in the renderer process.

However, shared workers don't yet support `navigator.serviceWorker`,
so they don't use many of the methods on `ServiceWorkerContainerHost`.
They also don't yet have a way to obtain a `ServiceWorkerRegistration`
or `ServiceWorker` JavaScript object.

#### Browser <-> Renderer (service worker)

For service workers, there are two message pipes: a) a "bootstrap" message
pipe for starting/stopping the service worker thread, and b) a message pipe
bound to the running service worker thread.

The "bootstrap" message pipe is established by the
`mojom.blink.EmbeddedWorkerInstanceClient` implementation in the renderer.  The
browser process uses this interface to ask the renderer process to start and
stop a service worker thread. The renderer process has a remote to a
corresponding `mojom.blink.EmbeddedWorkerInstanceHost` in the browser process.

In addition, like windows and shared workers above, the service worker has a
remote to a `mojom.blink.ServiceWorkerContainerHost` in the brocess process, and
the browser process has a remote to a `mojom.blink.ServiceWorkerContainer`.
These are on the bootstrap message pipe.

> Note: It's unclear why service workers use the `ServiceWorkerContainerHost`
> interface, because they are forbidden from calling any methods on this
> interface.  There are some plans to clean this up, see
> https://crbug.com/931087.

Running service worker threads have a dedicated message pipe, established by the
`mojom.blink.ServiceWorker` implementation. The browser process uses this
interface to ask the renderer to dispatch events to the service worker. The
service worker has a remote to a corresponding `mojom.blink.ServiceWorkerHost`
in the browser process.

Service workers have access to a `ServiceWorkerRegistration` JavaScript object
via `self.registration` and its `ServiceWorker` properties. The
`mojom.blink.ServiceWorker(Registration)Object(Host)` interfaces are bound to
the service worker thread's message pipe.

#### Renderer (window or shared worker) <-> Renderer (service worker)

Service worker clients in a renderer process have a direct connection to their
controller service worker, which can be in the same process or a different
process. The clients have a remote to a `mojom.blink.ControllerServiceWorker`
which they use to dispatch fetch events to the service worker.

This remote is given to the client by the browser process using
`SetController()` on the `mojom.blink.ServiceWorkerContainer` interface.  The
browser is the source of truth about which service worker is controlling which
client.

If the connection breaks, it likely means the service worker has stopped. The
service worker client asks the browser process to restart the service worker so
it can again dispatch fetch events to it.

## Directory structure

- [content/browser/service_worker]: Browser process code, including stored
  registration data, the inception of starting a service worker, and controlling
  navigations. The browser process has host objects of most live renderer
  entities that deal with service workers, and the bulk of work is performed by
  these host objects.
- [content/renderer/service_worker]: Renderer process code. This should move to
  third_party/blink per [Onion Soup].
- [third_party/blink/common/service_worker]: Common process code. Contains the
  implementation of [third_party/blink/public/common/service_worker].
- [third_party/blink/public/common/service_worker]: Header files for common
  process code that can be used by both inside Blink and outside Blink.
- [third_party/blink/public/mojom/service_worker]: Mojom files for common
  process code that can be used by both Blink and content.
- [third_party/blink/public/platform/modules/service_worker]: [Blink Public API]
  header files. This should be removed per [Onion Soup].
- [third_party/blink/public/web/modules/service_worker]: More [Blink Public API]
  header files. This should be removed per [Onion Soup].
- [third_party/blink/renderer/modules/service_worker]: Renderer process code in
  Blink. This is the closest code to the web-exposed Service Worker API.

## Storage

Service worker storage consists of the following.
- **Service worker registration metadata** is stored in a [LevelDB] instance
  located at ${DIR_USER_DATA}/Service Worker/Database.
- **Service worker scripts** are stored in a [disk_cache] instance using the
  "simple" implementation, located at ${DIR_USER_DATA}/Service
  Worker/ScriptCache. Registration metadata points to these scripts.

Code pointers include [ServiceWorkerDatabase] and [ServiceWorkerStorage].

The related [Cache Storage API] uses a [disk_cache] instance using the "simple"
implementation, located at ${DIR_USER_DATA}/Service Worker/CacheStorage. This
location was chosen because the [Cache Storage API] is currently defined in the
[Service Worker specification], but it can be used independently of service
workers.

For incognito windows, everything is in-memory.

### Eviction

Service workers storage lasts indefinitely, i.e, there is no periodic deletion
of old but still installed service workers. Installed service workers are only
evicted by the [Quota Manager] (or user action). The Quota Manager controls
several web platform APIs, including sandboxed filesystem, WebSQL,
IndexedDB, cache storage, service worker (registration and scripts), and
background fetch.

The Quota Manager starts eviction when one of the following conditions is true
(as of August 2018):
- **The global pool is full**: Chrome is using > 1/3 of the disk (>2/3 on CrOS).
- **The system is critically low on space**: the disk has < min(1GB,1%) free
  (regardless of how much Chrome is contributing!)

When eviction starts, origins are purged on an LRU basis until the triggering
condition no longer applies. Purging an origin deletes its storage completely.

Note that Quota Manager eviction is independent of HTTP cache eviction. The
HTTP cache is typically much smaller than the storage under the control of the
Quota Manager, and it likely uses a simple non-origin-based LRU algorithm.

## UseCounter integration

Blink has a UseCounter mechanism intended to measure the percentage of page
loads on the web that used a given feature.  Service workers complicate this
measurement because a feature use in a service worker potentially affects many
page loads, including ones in the future.

Therefore, service workers integrate with the UseCounter mechanism as follows:
- **If a feature use occurs before the service worker finished installing**, it
is recorded in storage along with the service worker. Any page thereafter that
the service worker controls is counted as using the feature.
- **If a feature use occurs after the service worker finished installing**, all
currently controlled pages are counted as using the feature.

For more details and rationale, see [Design of UseCounter for
workers](https://docs.google.com/document/d/1VyYZnhjBdk-MzCRAcX37TM5-yjwTY40U_J9rWnEAo8c/edit?usp=sharing)
and [crbug 376039](https://bugs.chromium.org/p/chromium/issues/detail?id=376039).

Code pointers include:
- (Browser -> Page) ServiceWorkerContainer.SetController and
ServiceWorkerContainer.CountFeature in [service_worker_container.mojom].
- (Service worker -> Browser) EmbeddedWorkerInstanceHost.CountFeature
in [embedded_worker.mojom].
- (Persistence) ServiceWorkerDatabase::RegistrationData::used_features
in [service_worker_database.h].

## Performance

We monitor service worker performance with real-world metrics
([UMA](/tools/metrics/histograms/README.md)) and performance benchmarks.

### UMA

The UMA data is internal-only. Key metrics include:

[Page load metrics](/chrome/browser/page_load_metrics/README) for service worker
controlled loads:
- PageLoad.Clients.ServiceWorker2.PaintTiming.NavigationToFirstContentfulPaint
- PageLoad.Clients.ServiceWorker2.Input.NavigationToFirstContentfulPaint

Service worker startup time and breakdown:
- ServiceWorker.StartWorker.Time
- ServiceWorker.StartTiming.Duration
- ServiceWorker.StartTiming.[A]To[B] (e.g.,
  ServiceWorker.StartTiming.StartToReceivedStartWorker)

Fetch event handling:
- ServiceWorker.LoadTiming.MainFrame.MainResource.\*
- ServiceWorker.LoadTiming.Subresource.\*

Service worker's startup sequence is composed of a few steps in
ServiceWorker.StartTiming.[A]To[B]. These are the milestones that can be in the
[A] and [B].

1. Start (browser)
2. SentStartWorker (browser)
3. ReceivedStartWorker (renderer)
4. ScriptEvaluationStart (renderer)
5. ScriptEvaluationEnd (renderer)
6. End (browser)

Here's the explanation about the each section:
- **Start to SentStartWorker**: the browser process initiates a starting
  worker sequence. This may include process creation if not exists, and setting up
  URLLoaderFactories.
- **SentStartWorker to ReceivedStartWorker**:  This section measures the IPC
  delay. SendStartWorker is recorded when the browser sends a Mojo message to
  start a worker thread to the renderer process. ReceivedStartWorker is
  recordred when the renderer receives it.
- **ReceivedStartWorker to ScriptEvaluationStart**: This measures the time to be
  spent for starting a worker thread, and preparation for the V8 isolate and
  context.
- **ScriptEvaluationStart to ScriptEvaluationEnd**: the initial script
  evaluation. This metrics can be affected by the content of service scripts.
- **ScriptEvaluationEnd to End**: This measures the IPC delay of OnStarted
   message from the renderer to the browser.

### Tests

We run a limited number of
[Telemetry](https://chromium.googlesource.com/catapult/+/HEAD/telemetry/README.md)
benchmark tests for service worker and a few microbenchmarks in
[blink_perf](https://chromium.googlesource.com/chromium/src/+/main/docs/speed/benchmark/harnesses/blink_perf.md#service-worker-perf-tests)
([crbug](https://crbug.com/1019097)).

Telemetry tests are part of the [Loading
benchmarks](/docs/speed/benchmark/harnesses/loading.md), as the "pwa" tests
inside the "loading.mobile" suite. The tests do not run on desktop machines
(loading.desktop) due to resource constraints.

See a quick
[dashboard](https://chromeperf.appspot.com/report?sid=59acafc01d33fa4fcea163b4b83d733670d91e7c2eaa853656c2e23f21c04dfd)
of these test results. You can also run the benchmarks locally:

```
# Run benchmark on `FlipKart`
$ tools/perf/run_benchmark --browser=android-chromium loading.mobile --story-filter='FlipKart'

# Run benchmark on `FlipKart` with cache_temperature = cold
$ tools/perf/run_benchmark --browser=android-chromium loading.mobile --story-filter='FlipKart_cold'
```

> TODO(falken): Merge this with loading.md and cache_temperature.py documentation.

The PWA tests load a page multiple times. Each time has a different "cache
temperature". These temperatures have special significance for service worker
controlled page loads:
- **cold**: tests the very first load to a page (no active service worker).
  Browser cache and storage data including service worker registrations are
  cleared first.
- **warm**: tests the second load to a page (with an active service worker). It
  first does a cold load which installs a service worker, waits for the service
  worker to reach activated state, and then tests the load.
- **hot**: tests the third load to the page (with an active service worker and
  V8 code caching). It first does a warm load, then waits(?) for V8 Code Caching
  to complete, then tests another load.

Service workers are terminated between loads in order to include service worker
startup as part of the performance test.

Code links and resources:
- PWA test suite: see 'pwa' in
  [loading_mobile.py](/tools/perf/page_sets/loading_mobile.py), as of March 2019
  [here](https://cs.chromium.org/chromium/src/tools/perf/page_sets/loading_mobile.py?l=88&rcl=e590d4e0ae6d3cbdabee199ea6fabe152a3eea83).
- [cache_temperature.py](https://chromium.googlesource.com/catapult/+/main/telemetry/telemetry/page/cache_temperature.py)
- "Perf benchmark for PWAs using the loading benchmark": [crbug](https://crbug.com/736697) and
  [design doc](https://docs.google.com/document/d/1Nf97CVp1X7aSqvAspyJ7yOCDyr1osUNrnfrGwZ_Yuuo/edit?usp=sharing).

## Other documentation

- [Service Worker Security FAQ](/docs/security/service-worker-security-faq.md)
- [ES Modules in Service Workers](/content/browser/service_worker/es_modules.md)
