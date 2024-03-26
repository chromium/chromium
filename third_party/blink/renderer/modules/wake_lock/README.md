# modules/wake_lock

[TOC]

This directory contains an implementation of the [Wake Lock specification], a Web API that allows script authors to prevent both the screen from turning off ("screen wake locks") as well as the CPU from entering a deep power state ("system wake locks"). There are platform implementations for ChromeOS, Linux (X11), Mac, Android, and Windows.

At the time of writing (October 2019), system wake lock requests are always denied, as allowing them depends on a proper permission model for the requests being figured out first.

The code required to implement the Wake Lock API is spread across multiple Chromium subsystems: Blink, `//content`, `//components`, `//services` and `//chrome`. This document focuses on the Blink part, and the other subsystems are mentioned when necessary but without much detail.

## High level overview

Wake Lock's API surface is fairly small: `Navigator` and `WorkerNavigator` provide a `wakeLock` attribute that exposes the `WakeLock`, an interface with a single method to request a wake lock, and a `WakeLockSentinel` can be used to both release the requested lock and receive an event when it is released. All the parts that actually communicate with platform APIs are implemented elsewhere, so the Blink side only exposes the JavaScript API to script authors, validates API calls and manages the [`[[ActiveLocks]]` internal slot](https://w3c.github.io/screen-wake-lock/#dfn-activelocks).

Wake Lock usage in scripts looks like this:

```javascript
let lock = await navigator.wakeLock.request("screen");
lock.addEventListener("release", (ev) => {
  console.log(`${ev.target.type} wake lock released`);
});
lock.release();
```

The three main Blink classes implementing the spec are:

* [`WakeLock`](wake_lock.h): per-`Navigator`/`WorkerNavigator` class that implements the bindings for the `WakeLock` IDL interface. Its responsibilities also include performing permission requests and [Wake Lock management] tasks that apply to documents and/or workers (e.g. page visibility handling). Lock acquisition calls are all forwarded to `WakeLockManager`. Its `managers_` array contains per-wake lock type `WakeLockManager` instances, so all wake lock types are managed independently.
* [`WakeLockManager`](wake_lock_manager.h): Owned by `WakeLock`. This is an implementation of the `[[ActiveLocks]]` internal slot added to the `Document` interface. Like in the spec, it keeps track of all active locks of a certain type, and it is also responsible for communicating with the `//content` and `//services` layers to request and cancel wake locks.
* [`WakeLockSentinel`](wake_lock_sentinel.h): Owned by `WakeLockManager`. This is an implementation of the `WakeLockSentinel` IDL interface that is used to both release a lock requested by `WakeLock.request()` and receive a `release` event when it is released (either by `WakeLockSentinel.release()` or due to a platform event, such as a loss of context, or a page visibility change in the case of screen wake locks). This class is an event target, so it inherits from [`ActiveScriptWrappable`](../../bindings/core/v8/active_script_wrappable.h) to avoid being garbage-collected while there is pending activity.

Furthermore, [`wake_lock.mojom`](../../../public/mojom/wake_lock/wake_lock.mojom) defines the Mojo interface implemented by `//content`'s [`WakeLockServiceImpl`](/content/browser/wake_lock/wake_lock_service_impl.h) that `WakeLockManager` uses to obtain a [`device::mojom::blink::WakeLock`](/services/device/public/mojom/wake_lock.mojom) and request/cancel a wake lock.

The rest of the implementation is found in the following directories:

* `content/browser/wake_lock` [implements](/content/browser/wake_lock/wake_lock_service_impl.cc) the [`WakeLockService`](../../../public/mojom/wake_lock/wake_lock.mojom) Mojo interface defined in Blink. It is responsible for communicating with Blink and connecting Blink to `services/device/wake_lock`.
* `services/device/wake_lock` contains the [platform-specific parts of the implementation](../../../../../services/device/wake_lock/power_save_blocker) and implements the Wake Lock [Mojo interfaces].
* `components/permissions/contexts` contains the permission management for Wake Locks. When the Blink implementation needs to either query or request permission for wake locks, the request bubbles up to this directory, where the decision is made based on the wake lock type (for testing purposes, `content_shell` always grants screen wake locks and denies system wake locks in [`shell_permission_manager.cc`](/content/shell/browser/web_test/web_test_message_filter.cc)).

[Mojo interfaces]: ../../../../../services/device/public/mojom/
[Wake Lock management]: https://w3c.github.io/screen-wake-lock/#managing-wake-locks
[Wake Lock specification]: https://w3c.github.io/screen-wake-lock/

### Testing

Validation, exception types, permissions policy integration and general IDL compliance are tested in [web platform tests].

Larger parts of the Blink implementation are tested as browser and unit tests:

* `*_test.cc` and `wake_lock_test_utils.{cc,h}` are built as part of the `blink_unittests` GN target, and attempt to have coverage over most of the code in this directory.
* The unit tests in `services/device/wake_lock` test the service side of the API implementation.
* `chrome/browser/wake_lock` has browser tests for end-to-end behavior testing.
* `components/permissions/contexts` has unit tests for `WakeLockPermissionContext`.
* content_shell implements its own permission logic that mimics what is done in `//chrome` in [`shell_permission_manager.cc`](/content/shell/browser/shell_permission_manager.cc).

[web platform tests]: ../../../web_tests/external/wpt/screen-wake-lock/

## Example workflows

### Wake Lock acquisition

This section describes how the classes described above interact when the following excerpt is run in the browser:

``` javascript
const lock = await navigator.wakeLock.request("screen");
```

1. `WakeLock::request()` performs all the validation steps described in [the spec](https://w3c.github.io/screen-wake-lock/#the-request-method). If all checks have passed, it creates a `ScriptPromiseResolverBase` and calls `WakeLock::DoRequest()`.
1. `WakeLock::DoRequest()` simply forwards its arguments to `WakeLock::ObtainPermission()`. It exists as a separate method just to make writing unit tests easier, as we'd otherwise be unable to use our own `ScriptPromiseResolverBase`s in tests.
1. `WakeLock::ObtainPermission()` connects to the [permission service](../../../public/mojom/permissions/permission.mojom) and asynchronously requests permission for a screen wake lock.
1. In the browser process, the permission request bubbles up through `//content` and reaches [`WakeLockPermissionContext`](/components/permissions/contexts/wake_lock_permission_context.cc), where `WakeLockPermissionContext::GetPermissionStatusInternal()` always grants `CONTENT_SETTINGS_TYPE_WAKE_LOCK_SCREEN` permission requests.
1. Back in Blink, the permission request callback in this case is `WakeLock::DidReceivePermissionResponse()`. It performs some sanity checks such as verifying if the page visibility changed while waiting for the permission request to be processed. If any of the checks fail, or if the permission request was denied, the `ScriptPromiseResolverBase` instance created earlier by `WakeLock::request()` is rejected and we stop here. If everything went well, `WakeLockManager::AcquireWakeLock()` is called.
1. If there are no existing screen wake locks, `WakeLockManager::AcquireWakeLock()` will connect to the `WakeLockService` Mojo interface, invoke its `GetWakeLock()` method to obtain a `device::mojom::blink::WakeLock` and call its `RequestWakeLock()` method.
1. `WakeLockManager::AcquireWakeLock()` creates a new `WakeLockSentinel` instance, passing `this` as the `WakeLockSentinel`'s `WakeLockManager`. This new `WakeLockSentinel` is added to its set of [active locks].
1. The `ScriptPromiseResolverBase` created by `WakeLock::request()` is resolved with the new `WakeLockSentinel` object.

[active locks]: https://w3c.github.io/screen-wake-lock/#dfn-activelocks

### Wake Lock cancellation

Given the excerpt below:

``` javascript
const lock = await navigator.wakeLock.request("screen");
await lock.release();
```

This section describes what happens when `lock.release()` is called.

1. `lock.release()` results in a call to `WakeLockSentinel::release()`.
1. `WakeLockSentinel::release()` calls `WakeLockSentinel::DoRelease()` and returns a resolved promise. `WakeLockSentinel::DoRelease()` exists as a separate method because it is also called directly by `WakeLockManager::ClearWakeLocks()` when an event such as a page visibility change causes all screen wake locks to be released.
1. `WakeLockSentinel::DoRelease()` aborts early if its `manager_` member is not set. This can happen if `WakeLock::release()` has already been called before, or if `WakeLockManager::ClearWakeLocks()` has already released this `WakeLockSentinel`.
1. `WakeLockSentinel::DoRelease()` calls `WakeLockManager::UnregisterSentinel()`.
1. `WakeLockManager::UnregisterSentinel()` implements the spec's [release wake lock algorithm]. If the given `WakeLockSentinel` is in `WakeLockManager`'s `wake_lock_sentinels_`, it will be removed and, if the list is empty, `WakeLockManager` will communicate with its `device::mojom::blink::WakeLock` instance and call its `CancelWakeLock()` method.
1. Back in `WakeLockSentinel::DoRelease()`, it then clears its `manager_` member, and dispatches a `release` event with itself as a target.

[release wake lock algorithm]: https://w3c.github.io/screen-wake-lock/#release-wake-lock-algorithm

## Other Wake Lock usage in Chromium

### Inside Blink

Video playback via the `<video>` tag currently uses a screen wake lock behind the scenes to prevent the screen from turning off while a video is being played. The implementation can be found in the [`VideoWakeLock`](../../core/html/media/video_wake_lock.h) class.

This is an implementation detail, but the code handling wake locks in `VideoWakeLock` is similar to `WakeLockManager`'s, where Blink needs to talk to `//content` to connect to a `WakeLockServiceImpl` and use that to get to a `device::mojom::blink::WakeLock`.

**Note:** when writing new code that uses Wake Locks in Blink, it is recommended to follow the same pattern outlined above. That is, connect to `WakeLockService` via the `//content` layer and request a `device::mojom::blink::WakeLock` via `WakeLockService::GetWakeLock()`. Do not go through the classes in this module, and [**do not connect to the Wake Lock services directly**](/docs/servicification.md#frame_scoped-connections). See the example below:

```c++
mojo::Remote<mojom::blink::WakeLockService> wake_lock_service;
mojo::Remote<device::mojom::blink::WakeLock> wake_lock;
execution_context->GetInterface(wake_lock_service.BindNewPipeAndPassReceiver());
wake_lock_service->GetWakeLock(..., wake_lock.BindNewPipeAndPassReceiver());
wake_lock_->RequestWakeLock();
```

### Outside Blink

The [Wake Lock service](/services/device/wake_lock) is also used by multiple parts of Chromium outside Blink. In other words, it is possible to use the Wake Lock service and prevent screen and CPU from entering a deep power state directly from the browser side.

In fact, this is why [`WakeLockProvider::GetWakeLockWithoutContext()`](/services/device/public/mojom/wake_lock_provider.mojom) exists in the first place. One consequence is that one needs to bear in mind these other usages when changing the public API exposed by the Wake Lock service.

**Note:** Avoid using `WakeLockProvider::GetWakeLockWithoutContext()` whenever possible. By design, it does not work on Android.

Example usage outside Blink includes:

* ChromeOS's [encryption migration screen handler](/chrome/browser/ui/webui/ash/login/encryption_migration_screen_handler.cc)
* Media capture code in [content/browser](/content/browser/media/capture/desktop_capture_device.cc)
* The Google Drive [component](/components/drive/drive_uploader.cc)
* The `chrome.power` [extension API](/extensions/browser/api/power/power_api.cc)

## Permission Model

The Wake Lock API spec checks for user activation in the context of [wake lock permission requests](https://w3c.github.io/screen-wake-lock/#dfn-obtain-permission), as a result of a call to `WakeLock.request()`. If a user agent is configured to prompt a user when a wake lock is requested, user activation is required, otherwise the request will be denied.

In the Chromium implementation, there currently is no "prompt" state, and no permission UI or settings: wake lock requests are either always granted or always denied:

* Screen wake lock request are always granted without prompting or user activation checks. This is based on the existing precedent of the `<video>` tag's use of `VideoWakeLock`s: they are always requested and granted transparently, so even if the Wake Lock API implementation in Chromium started requiring stricter checks, malicious actors could still embed a `<video>` tag and prevent the screen from turning off without any user interaction.

* System wake lock requests are always denied in `components/permissions/contexts/wake_lock_permission_context.cc`. This means the entirety of the code is present and enabled in Blink, but all calls to `WakeLock.request('system')` currently return a promise that will be rejected with a `NotAllowedError`. Changing that requires figuring out a permission model for system wake lock requests, which, at the moment, is future work.
