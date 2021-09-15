# Message passing API

`dispatcher.js` (and its server-side backend `dispatcher.py`) provides a
universal queue-based message passing API.
Each queue is identified by a UUID, and accessed via the following APIs:

-   `send(uuid, message)` pushes a string `message` to the queue `uuid`.
-   `receive(uuid)` pops the first item from the queue `uuid`.
-   `showRequestHeaders(origin, uuid)` and
    `cacheableShowRequestHeaders(origin, uuid)` return URLs, that push request
    headers to the queue `uuid` upon fetching.

It works cross-origin, and even access different browser context groups.

Messages are queued, this means one doesn't need to wait for the receiver to
listen, before sending the first message
(but still need to wait for the resolution of the promise returned by `send()`
to ensure the order between `send()`s).

# Executor framework

The message passing API can be used for sending arbitrary javascript to be
evaluated in another page or worker (the "executor").

`executor.html` (as a Document), `executor-worker.js` (as a Web Worker), and
`executor-service-worker.js` (as a Service Worker) are examples of executors.
Tests can send arbitrary javascript to these executors to evaluate in its
execution context.

This is universal and avoids introducing many specific `XXX-helper.html`
resources.
Moreover, tests are easier to read, because the whole logic of the test can be
defined in a single file.
