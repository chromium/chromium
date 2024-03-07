# Network Service

[TOC]

This is a service for networking. It's meant to be oblivious to Chrome's features.
Some design goals
  * this only contains features that go over the network. e.g. no file loading, data URLs etc...
  * only the lowest-level of networking should be here. e.g. http, sockets, web
    sockets. Anything that is built on top of this should be in higher layers.
  * higher level web platform and browser features should be built outside of
    this code. Safe browsing, Service Worker, extensions, devtools etc... should
    not have hooks here. The only exception is when it's impossible for these
    features to function without some hooks in the network service. In that
    case, we add the minimal code required. Some examples included traffic
    shaping for devtools, ORB blocking, and CORS.
  * every PostTask, thread hop and process hop (IPC) should be counted carefully
    as they introduce delays which could harm this performance critical code.
  * `NetworkContext` and `NetworkService` are trusted interfaces that aren't
    meant to be sent to the renderer. Only the browser should have access to
    them.

See https://bugs.chromium.org/p/chromium/issues/detail?id=598073

See the design doc
https://docs.google.com/document/d/1wAHLw9h7gGuqJNCgG1mP1BmLtCGfZ2pys-PdZQ1vg7M/edit?pref=2&pli=1#

# Related docs

* [URLLoader](url_loader.md)
* [Slides describing the relationship with the fetch spec](https://docs.google.com/presentation/d/1r9KHuYbNlgqQ6UABAMiWz0ONTpSTnMaDJ8UeYZGWjls/)

# Where does the network service run?

> Note: For more background about this section, see also [Multi-process
Architecture](https://www.chromium.org/developers/design-documents/multi-process-architecture)
for an overview of the processes in Chromium.

The network service is designed as a [Mojo service](/docs/mojo_and_services.md)
that in general doesn't need to be aware of which thread/process it runs on.
The browser process launches the network service and decides whether to run it
inside the browser process (*in-process*) or in a dedicated utility process
(*out-of-process*).

The out-of-process configuration is preferred for isolation and stability, and
is the default on most platforms. The in-process configuration is the default on
Android because of some unresolved issues; see https://crbug.com/1049008.  It
can also be useful for debugging; for example, it's used in Chromium's
[`--single-process`](https://www.chromium.org/developers/design-documents/process-models)
mode.

*In the out-of-process case*: The network service runs on the [IO
thread](/docs/threading_and_tasks.md) of the utility process (see this
[comment][1] in `content/utility/services.cc` for why). The utility process
houses only the network service, so there is nothing running on its main thread.

[1]: https://source.chromium.org/chromium/chromium/src/+/main:content/utility/services.cc;l=197-198;drc=9b85cd82c52e13ed685dd74c726d91067bbd34d5

*In the in-process case*: The network service runs on its own dedicated thread
in the browser process. Exception: on Chrome OS, it currently runs on the IO
thread; see https://crbug.com/1086738.

# How does the network service start?

*In the out-of-process case*: The browser creates the utility process and asks
it to launch the network service. For the browser-side code, see
`GetNetworkService()` in `content/browser/network_service_instance_impl.cc`.
For the utility process code, see `GetIOThreadServiceFactory` in
content/utility/services.cc. This calls `RunNetworkService()` which creates the
`network::NetworkService` instance. For more background about Chromium's
services architecture, see [Mojo and Services](/docs/mojo_and_services.md).

*In the in-process case*: The browser process starts the network service. See
`CreateInProcessNetworkService()` in
`content/browser/network_service_instance_impl.cc`, which posts a task to create
the `network::NetworkService` instance.

# What happens if the network service crashes?

*In the out-of-process case*: If the network service crashes, it gets restarted
in a new utility process. The goal is for the failure to be mostly recoverable.
It is important to note that any URLLoaderFactories bound to the Network Service
before it crashes become disconnected, and will no longer continue to work.
Therefore it is useful to establish reconnection logic if it is detected that
the URLLoaderFactory is no longer connected.

For example, a navigation request's URLLoaderFactory comes from
`StoragePartitionImpl::GetURLLoaderFactoryForBrowserProcessInternal` in the
browser process. This method has logic to detect if the URLLoaderFactory it
would normally return is disconnected. In that case, it creates a new one which
is used for all future navigation requests. Since most URLLoaderFactory users
use factories that are not created out-of-band, and are provided by some
service, reconnection logic is often implemented for free, and is usually not
something to worry about.

*In the in-process case*: If the network service crashes in this case, of
course, the entire browser crashes. This is one reason for the goal to always
run it out-of-process.

# Buildbot

The [Network Service
Linux](https://ci.chromium.org/p/chromium/builders/ci/Network%20Service%20Linux)
buildbot runs browser tests with the network service in non-default but
supported configurations. Ideally this bot would be on the CQ, but it is
expensive and would affect CQ time, so it's on the main waterfall but not the
CQ.

Its steps are:

* **`network_service_in_process_browser_tests`**: Runs `browser_tests` with the
  network service in-process
  (`--enable-features=NetworkServiceInProcess`). This step is important because
  Chrome on Android runs with the network service in-process by default
  (https://crbug.com/1049008). However, `browser_tests` are not well-supported
  on Android (https://crbug.com/611756), so we run them on this Linux bot.
* **`network_service_in_process_content_browsertests`**: Same as above but for
  `content_browsertests`. We might consider removing this from the bot, since
  the Android bots run `content_browsertests` which should give enough coverage.
* **`network_service_web_request_proxy_browser_tests`**: Runs `browser_tests`
  while forcing the "network request proxying" code path that is taken when the
  browser has an extension installed that uses the
  [Web Request API](https://developer.chrome.com/extensions/webRequest)
  (`--enable-features=ForceWebRequestProxyForTest`). This step has caught bugs
  that would be Stable Release Blockers, so it's important to keep it.
