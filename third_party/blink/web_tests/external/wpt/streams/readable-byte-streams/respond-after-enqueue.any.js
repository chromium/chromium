// META: global=window,worker,jsshell

'use strict';

// Repro for Blink bug https://crbug.com/1255762.
promise_test(async () => {
  const rs = new ReadableStream({
    type: 'bytes',
    autoAllocateChunkSize: 10,
    pull(controller) {
      controller.enqueue(new Uint8Array([1, 2, 3]));
      controller.byobRequest.respond(10);
    }
  });

  const reader = rs.getReader();
  const {value, done} = await reader.read();
  assert_false(done, 'done should not be true');
  assert_array_equals(value, [1, 2, 3], 'value should be 3 bytes');
}, 'byobRequest.respond() after enqueue() should not crash');

promise_test(async () => {
  const rs = new ReadableStream({
    type: 'bytes',
    autoAllocateChunkSize: 10,
    pull(controller) {
      const byobRequest = controller.byobRequest;
      controller.enqueue(new Uint8Array([1, 2, 3]));
      byobRequest.respond(10);
    }
  });

  const reader = rs.getReader();
  const {value, done} = await reader.read();
  assert_false(done, 'done should not be true');
  assert_array_equals(value, [1, 2, 3], 'value should be 3 bytes');
}, 'byobRequest.respond() with cached byobRequest after enqueue() should not crash');
