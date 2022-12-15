# Extension events

The Chrome extensions system has its own implementation of events (typically
exposed as `chrome.<api>.onFoo`, e.g. `chrome.tabs.onUpdated`). This doc
provides some notes about its implementation, specifically how event listeners
are registered and how they are dispatched.

## High level overview of extension event dispatching

An event listener registered in the renderer process is sent to the browser
process (via IPC). The browser process stores the listener information in
`EventListenerMap`. Events are dispatched from the browser process to the
renderer process via IPC. If the browser process requires persistence of any
listener, it does so by storing the listener information in `ExtensionPrefs`.

## Relevant concepts

* __Listener contexts__:
Typically denotes the page/script where the listener is registered from, e.g.
an extension's background page or an extension's service worker script.

* __Lazy contexts__:
Contexts that are not persistent and typically shut down when inactive, e.g. an
event page's background script or an extension service worker script. Non-lazy
contexts are often called "persistent" contexts.

* __Persistent listeners / Non-lazy listeners__:
Listeners from contexts that are not lazy.

* __Lazy listeners__:
Listeners from lazy context.
See the scenario description (_Case 1_ and _Case 2_) below for quick explanation
of how registration of a listener from a lazy context can result in two (a lazy
and a non-lazy) listeners. An event can be dispatched to these listeners while
the corresponding lazy context is not running.

* __Filtered events__:
A listener can specify additional matching criteria that we call event filters.
Some events support filters. IPCs (along with most but not all of the browser/
or renderer/ code) use `DictionaryValue` to represent an event filter.


## Event listener registration

Event listeners are registered in JavaScript in the renderer process. The
event bindings code handles this registration and the browser process is made
aware of it via IPC.

In particular, a message filter (`ExtensionMessageFilter`) receives event
registration IPCs and it passes them to `EventRouter` to be stored in
`EventListenerMap`. If the listener is required to be persisted (for lazy
events), they are also recorded in `ExtensionPrefs`.

Note that when the renderer context is shut down, it removes the listener. The
exception is lazy event listener, which is not removed.


### Additional notes about lazy listeners

When a lazy listener is added for an event, a regular (non-lazy) listener
(call it `L1`) is added for it and in addition to that, a lazy variant of the
listener (call it `L2`) is also added. `L2` helps browser process remember that
the listener should be persisted and it should have lazy behavior.

## Event dispatching

`EventRouter` is responsible for dispatching events from the browser process.
When an event is required to be dispatched, `EventRouter` fetches EventListeners
from `EventListenerMap` and dispatches them to appropriate contexts (renderer
or service worker scripts)

### Additional notes about lazy event dispatching

Recall that a lazy listener is like a regular listener, except that it is
registered from a lazy context. A lazy context can be shut down. If an
interesting event occurs while a lazy context (with a listener to that event)
is no longer running, then the lazy context is woken up to dispatch the event.

The following (simplified) steps describe how dispatch is performed.

#### Case 1: Event dispatched while context (lazy or non-lazy) is running

* Because `EventListenerMap` will contain an entry for the listener (`L1`), it
will dispatch the event in normal fashion: by sending an IPC to the renderer
through `ExtensionMessageFilter`.

#### Case 2: Event dispatched while (lazy) context is not running

* If the context is not running, then `EventListenerMap` will not have any entry
for `L1` (because context shutdown will remove `L1`), but it will have an entry
for the lazy version of it, `L2`. Note that `L2` will exist even if the browser
process is restarted, `EventRouter::OnExtensionLoaded` will have loaded these
lazy events through `EventListenerMap::Load(Un)FilteredLazyListeners`.

* Realize that `L2` is lazy, so wake up its lazy context. Waking up an event
page context entails spinning up its background page, while waking up a service
worker context means starting the service worker.

* The lazy context will register `L1` and `L2` again, because the same code that
added the initial listeners will run again. This is an important step that
isn't intuitive. Note that `L2`, since it already exists in the browser process,
is not re-added.

* Dispatch `L1` (same as _Case 1_ above).

## Notes about extension service worker (ESW) events

* ESW events behave similar to event page events, i.e. lazy events.

* ESW events are registered from worker threads, instead of main renderer
threads.

* Similarly, event dispatch target is worker thread instead of main renderer
thread. Therefore, at dispatch time, browser process knows about the worker
thread
id in a RenderProcessHost. This is why worker event listener IPCs have
`worker_thread_id` param in them.

## TODOs

* Explain filters a bit more, where filter matching is performed and how much of
it lives in the renderer/ process.

* Describe what "manual" removal of event listeners means.
