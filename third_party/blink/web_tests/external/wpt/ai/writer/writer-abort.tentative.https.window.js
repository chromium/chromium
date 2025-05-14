// META: title=Writer Abort
// META: script=../resources/util.js
// META: timeout=long

'use strict';

promise_test(async t => {
  await testAbortPromise(t, signal => {
    return Writer.create({ signal: signal });
  });
}, "Aborting Writer.create().");

promise_test(async t => {
  const writer = await Writer.create();
  await testAbortPromise(t, signal => {
    return writer.write(kTestPrompt, { signal: signal });
  });
}, "Aborting Writer.write().");

promise_test(async t => {
  const writer = await Writer.create();
  await testAbortReadableStream(t, signal => {
    return writer.writeStreaming(kTestPrompt, { signal: signal });
  });
}, "Aborting Writer.writeStreaming().");

promise_test(async (t) => {
  const writer = await Writer.create();
  const controller = new AbortController();
  const streamingResponse = writer.writeStreaming(kTestPrompt, {
    signal: controller.signal,
    context: kTestContext,
  });
  for await (const chunk of streamingResponse) { /* Do nothing */}
  controller.abort();
}, 'Aborting Writer.writeStreaming() after finished reading.');
