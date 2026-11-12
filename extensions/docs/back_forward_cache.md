# Back/Forward Cache (BFCache) for Extension Frames

This document explains how component extensions opt into Back/Forward Cache
(**BFCache**) support for embedded extension frames (`<iframe>` elements) and
the behavioral consequences of opting in.

## Overview

By default, embedding an extension subframe (`<iframe
src="chrome-extension://...">`) in a web page blocks the main page from
entering BFCache. Top-level `chrome-extension://` main frames are also not
eligible for BFCache (which only caches HTTP/HTTPS main frames); by contrast,
web pages with injected extension content scripts are cached by default.

The default restriction on extension subframes exists for two reasons:

1. **Process-wide `WindowAgent` sharing:** By default, all same-origin
   extension frames in a renderer process share a single `WindowAgent` and
   event loop across all tabs in a given profile. When a tab enters BFCache,
   Blink pauses microtasks on its event loop. If a cached extension subframe
   shared that event loop with an active foreground tab or side panel, pausing
   the cached tab would freeze microtask execution across active extension
   contexts as well (because of this, extension subframes were blocked from
   entering BFCache in [crrev.com/c/7774414](https://crrev.com/c/7774414)).
2. **Stale extension state and events:** Cached extension frames remain alive
   in memory while frozen. Without explicit lifecycle handling, cached frames
   would miss extension events they may be expecting or hold stale messaging
   connections to extension background contexts.

Component extensions that embed subframes into web pages can opt into BFCache
support via the `"back_forward_cache"` manifest key:

```json
{
  "name": "Example Component Extension",
  "manifest_version": 3,
  "back_forward_cache": {
    "supported": true
  }
}
```

NOTE: This opt-in is currently restricted to component extensions (and gated by
the `BackForwardCacheForComponentExtensionFrames` feature flag) during initial
development and vetting. If support is expanded to third-party extensions in
the future, the behavioral differences below describe the contract extension
frames operate under when opted in.

## Behavioral differences

### 1. Per-`BrowsingInstance` `WindowAgent` isolation

Per the HTML specification, mutually reachable same-origin `Window` objects
share a `WindowAgent` so that synchronous cross-window scripting shares a
single event loop (including the microtask queue, `MutationObserver` delivery,
Custom Element reactions, and IndexedDB transaction cleanup) and agent cluster
ID. Because extension views across unrelated tabs can synchronously script each
other (for example, via `chrome.extension.getViews()` or named frame lookup),
extensions normally share a single `WindowAgent` across the entire renderer
process.

When an extension opts into BFCache, each `BrowsingInstance` (such as an
independent tab or side panel) is allocated its own `WindowAgent` and event
loop. Frames within the same tab, or windows linked via `window.open()`, still
share a `BrowsingInstance` and therefore share a `WindowAgent`.

Isolating the `WindowAgent` per `BrowsingInstance` has the following behavioral
consequences across unrelated tabs:

| Capability | Default extension behavior | BFCache opted-in behavior |
| :--- | :--- | :--- |
| **Microtask pausing on BFCache entry** | Pausing one tab's event loop pauses microtasks across all same-process tabs and side panels. | Pausing a cached tab's event loop only affects that `BrowsingInstance`; active tabs and side panels continue running microtasks normally. |
| **Synchronous cross-view DOM access (`getViews()`, named frames)** | Allowed across all same-process views. | **Still allowed.** Cross-`BrowsingInstance` access continues to work, though synchronous calls from View A into View B enqueue microtasks and DOM reactions on View B's queue rather than a single shared queue. |
| **Agent-cluster-locked `postMessage` payloads** | Transferable across unrelated tabs because all views share an agent cluster ID. | **Blocked across unrelated tabs.** Objects locked to an agent cluster (`SharedArrayBuffer`, `WebAssembly.Module`, transferable `RTCDataChannel`, `MediaSourceHandle`, and WebCodecs `AudioData`/`VideoFrame`) cannot be sent via `BroadcastChannel` or `MessagePort` between unrelated tabs (`messageerror` is fired, matching Manifest V3 service worker behavior). |
| **Main thread execution** | Shared across all same-process main-thread extension contexts (frames and views; service workers and other workers run on separate worker threads). | **Still shared.** Separate event loops isolate microtask queues, but all same-process main-thread extension contexts still share the renderer main thread. Synchronous JavaScript or thread-blocking calls like `window.alert()` still block other main-thread extension contexts in that process. |
| **Opening related windows (`window.open()`, `chrome.windows.create()`)** | Shares the process-wide `WindowAgent`. | Opening a window in a new `BrowsingInstance` (such as `chrome.windows.create()` with default options) allocates a separate `WindowAgent` and leaves the opener tab eligible for BFCache. Passing `setSelfAsOpener: true` or calling `window.open()` shares the opener's `BrowsingInstance` and blocks BFCache while the related window remains open. |

Additionally, `chrome.extension.getViews()` only returns top-level extension
main frames. Because BFCache only caches HTTP/HTTPS main frames and never
caches top-level `chrome-extension://` tabs, `getViews()` never returns a
BFCached extension frame.

### 2. Extension API events are dropped while cached

While a page containing an opted-in extension subframe is in BFCache, extension
API events (`chrome.<api>.on*`, such as `chrome.tabs.onCreated` or
`chrome.storage.onChanged`) targeted at that cached extension frame are
**dropped**:

- **No event queuing:** Pages can remain in BFCache for extended periods.
  Rather than queuing an unbounded number of stale events and firing them in a
  burst on restore, events dispatched while the frame is in BFCache are dropped
  for that frame. When a frame is restored from BFCache (`pageshow` with
  `event.persisted === true`), it may have missed events fired while it was
  cached.
- **Active frames are unaffected:** If an extension has both a cached subframe
  in Tab 1 and an active subframe or side panel in Tab 2, the active context in
  Tab 2 receives all events normally while Tab 1 drops them. Once Tab 1 is
  restored from BFCache, it resumes receiving subsequent events.
- **Content scripts vs. extension frames:** Event dropping applies only to
  extension frame contexts (`chrome-extension://` documents). For content
  scripts (`kContentScript` / `kUserScript`), extension API events such as
  `chrome.storage.onChanged` are queued by Blink's `PausableScriptExecutor`
  while the host page is frozen (without running JS or evicting the page) and
  fire if the page is restored from BFCache.

### 3. Messaging and `chrome.runtime.Port` lifecycle

Extension messaging APIs (`chrome.runtime.connect()`,
`chrome.runtime.sendMessage()`, and `chrome.tabs.connect()`) behave as follows
with BFCache:

1. **Entering BFCache closes active ports:** When a tab enters BFCache, the
   browser closes any open `chrome.runtime.Port` channels associated with
   frames under that page. The other endpoint (such as the extension service
   worker) receives an `onDisconnect` event indicating that the page moved into
   BFCache.
2. **Messages to cached frames are dropped:** In-flight messages or connection
   attempts targeted at a frame already in BFCache are not delivered to the
   cached frame.
3. **Ports stay disconnected on restore:** Closed `Port` objects cannot be
   reopened and are not automatically reconnected when the page is restored;
   frames that need a persistent channel after restoration must open a new port
   (for example, calling `chrome.runtime.connect()` on `pageshow` when
   `event.persisted` is `true`).

### 4. Extension unload evicts cached pages

If an opted-in extension is unloaded, disabled, or reloaded while a web page
hosting one of its subframes is in BFCache, the browser evicts the cached page
and destroys the extension frame rather than restoring a page with a defunct
extension subframe.

## FAQ

### Why can't a web page or content script just remove the extension `<iframe>` in `pagehide` instead of opting in?

Removing an `<iframe>` or navigating it to `about:blank` inside a `pagehide`
listener does not make the page eligible for BFCache:

1. For extensions that have not opted in, the browser records the
   `kExtensionFrame` disablement reason on the extension subframe's
   `RenderFrameHost` as soon as the subframe is registered, and that reason
   persists across same-`RenderFrameHost` navigations (such as navigating to
   `about:blank`).
2. During navigation away from the page, the browser evaluates BFCache
   eligibility across all `RenderFrameHost`s in the frame tree before renderer
   `pagehide` DOM mutations (such as frame detach) are processed.
3. Even if an extension has opted in, detaching a subframe or starting a
   subframe navigation inside `pagehide` runs after the browser has already
   placed the page into BFCache, causing the browser to evict the cached page
   (`kDetach`, `kBeginNavigation`, or `kDidCommitNavigation`).
