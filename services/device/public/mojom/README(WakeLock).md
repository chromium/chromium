WakeLock provides the ability to block the device / display from sleeping.

On Android, the implementation is inherently coupled to the NativeView
associated with the context of the requestor due to system APIs. To handle
this coupling, the Wake Lock usage model on Android is as follows:

(1) The embedder passes in a callback at Device Service construction that
enables the Wake Lock implementation to map (embedder-specific) context IDs to
NativeViews.
(2) For a given embedder-specific context, a trusted client
connects to the WakeLockProvider interface and gets a
WakeLockContext instance that is associated with that context.
(3) That trusted client then forwards requests to bind wake locks from
untrusted clients that are made within that context, with the Wake Lock
implementation using the callback from (1) as necessary to obtain the
NativeView associated with that context.

On other platforms, the usage model is similar but the callback is not
necessary/employed.

If the client does not have any context available (e.g., is not within the
context of a WebContents), it can get a WakeLock that doesn't associate to any
context (by GetWakeLockWithoutContext() in WakeLockProvider). On Android, a
contextless WakeLock requires the embedder to hold the WAKE_LOCK permission.
