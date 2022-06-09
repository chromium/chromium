importScripts("/resources/testharness.js");

test(t => {
  // The Window test html conditionally fetches and runs these tests only if the
  // implementation exposes a true-valued static canConstructInDedicatedWorker
  // attribute on MediaSource in the Window context. So, the implementation must
  // agree on support here in the dedicated worker context.

  // Ensure we're executing in a dedicated worker context.
  assert_true(self instanceof DedicatedWorkerGlobalScope, "self instanceof DedicatedWorkerGlobalScope");
  assert_true(MediaSource.hasOwnProperty("canConstructInDedicatedWorker", "DedicatedWorker MediaSource hasOwnProperty 'canConstructInDedicatedWorker'"));
  assert_true(MediaSource.canConstructInDedicatedWorker, "DedicatedWorker MediaSource.canConstructInDedicatedWorker");
}, "MediaSource in DedicatedWorker context must have true-valued canConstructInDedicatedWorker if Window context had it");

test(t => {
  assert_true("getHandle" in MediaSource.prototype, "dedicated worker MediaSource must have getHandle");
  assert_true(self.hasOwnProperty("MediaSourceHandle"), "dedicated worker must have MediaSourceHandle visibility");
}, "MediaSource prototype in DedicatedWorker context must have getHandle, and worker must have MediaSourceHandle");

test(t => {
  const ms = new MediaSource();
  assert_equals(ms.readyState, "closed");
}, "MediaSource construction succeeds with initial closed readyState in DedicatedWorker");

test(t => {
  const ms = new MediaSource();
  const handle = ms.getHandle();
  assert_not_equals(handle, null, "must have a non-null getHandle result");
  assert_true(handle instanceof MediaSourceHandle, "must be a MediaSourceHandle");
}, "mediaSource.getHandle() in DedicatedWorker returns a MediaSourceHandle");

test(t => {
  const ms = new MediaSource();
  const handle1 = ms.getHandle();
  let handle2 = null;
  assert_throws_dom("InvalidStateError", function()
    {
      handle2 = ms.getHandle();
    }, "getting second handle from MediaSource instance");
  assert_equals(handle2, null, "getting second handle from same MediaSource must have failed");
  assert_not_equals(handle1, null, "must have a non-null result of the first getHandle");
  assert_true(handle1 instanceof MediaSourceHandle, "first getHandle result must be a MediaSourceHandle");
}, "mediaSource.getHandle() must not succeed more than precisely once for a MediaSource instance");

done();
