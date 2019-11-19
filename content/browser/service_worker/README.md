# Service workers
[content/browser/service_worker]: /content/browser/service_worker
[content/renderer/service_worker]: /content/renderer/service_worker
[content/renderer/service_worker]: /content/renderer/service_worker
[content/common/service_worker]: /content/common/service_worker
[disk_cache]: /net/disk_cache/README.md
[embedded_worker.mojom]: https://codesearch.chromium.org/chromium/src/third_party/blink/public/mojom/service_worker/embedded_worker.mojom
[service_worker_container.mojom]: https://codesearch.chromium.org/chromium/src/third_party/blink/public/mojom/service_worker/service_worker_container.mojom
[service_worker_database.h]: https://codesearch.chromium.org/chromium/src/content/browser/service_worker/service_worker_database.h
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
[ServiceWorkerDatabase]: https://codesearch.chromium.org/chromium/src/content/browser/service_worker/service_worker_database.h
[ServiceWorkerStorage]: https://codesearch.chromium.org/chromium/src/content/browser/service_worker/service_worker_storage.h
[Service Worker specification]: https://w3c.github.io/ServiceWorker/

This is Chromium's implementation of [service
workers](https://developer.mozilla.org/en-US/docs/Web/API/Service_Worker_API).
See the [Service Worker specification].

## Directory structure

- [content/browser/service_worker]: Browser process code, including stored
  registration data, the inception of starting a service worker, and controlling
  navigations. The browser process has host objects of most live renderer
  entities that deal with service workers, and the bulk of work is performed by
  these host objects.
- [content/renderer/service_worker]: Renderer process code. This should move to
  third_party/blink per [Onion Soup].
- [content/common/service_worker]: Common process code.
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
several web platform APIs, including sandboxed filesystem, WebSQL, appcache,
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
- PageLoad.Clients.ServiceWorker2.InteractiveTiming.FirstInputDelay3

Service worker startup time and breakdown:
- ServiceWorker.StartWorker.Time
- ServiceWorker.StartTiming.Duration
- ServiceWorker.StartTiming.\*To\* (e.g.,
  ServiceWorker.StartTiming.StartToReceivedStartWorker)

Fetch event handling:
- ServiceWorker.LoadTiming.MainFrame.MainResource.\*
- ServiceWorker.LoadTiming.Subresource.\*

TODO(falken, bashi): Add a list of the milestones of startup and fetch event
handling.

### Tests

We run a limited number of
[Telemetry](https://chromium.googlesource.com/catapult/+/HEAD/telemetry/README.md)
benchmark tests for service worker. These tests are part of the [Loading
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

TODO(falken): Merge this with loading.md and cache_temperature.py documentation.

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
- [cache_temperature.py](https://chromium.googlesource.com/catapult/+/master/telemetry/telemetry/page/cache_temperature.py)
- "Perf benchmark for PWAs using the loading benchmark": [crbug](https://crbug.com/736697) and
  [design doc](https://docs.google.com/document/d/1Nf97CVp1X7aSqvAspyJ7yOCDyr1osUNrnfrGwZ_Yuuo/edit?usp=sharing).

## Other documentation

- [Service Worker Security FAQ](/docs/security/service-worker-security-faq.md)
