// META: title=Language Model Prompt Context Overflow
// META: script=/resources/testdriver.js
// META: script=/resources/testdriver-vendor.js
// META: script=../../../resources/util.js
// META: timeout=long

'use strict';

promise_test(async () => {
  await ensureLanguageModel();
  const session = await createLanguageModel();
  const listenerPromise = new Promise(resolve => {
    session.addEventListener('contextoverflow', resolve);
  });
  const handlerPromise = new Promise(resolve => {
    session.oncontextoverflow = resolve;
  });
  // Make sure there is something to evict.
  const kLongPrompt = kTestPrompt.repeat(10);
  const usage = await session.measureContextUsage(kLongPrompt);
  assert_greater_than(session.contextWindow, usage);
  await session.append(kLongPrompt);
  assert_greater_than(session.contextUsage, 0);
  // Generate a repeated kLongPrompt string that exceeds contextWindow.
  const repeatCount = session.contextWindow / session.contextUsage;
  const promptString = kLongPrompt.repeat(repeatCount);
  // The prompt promise succeeds, while causing older input to be evicted.
  const [listenerEvent, handlerEvent] = await Promise.all(
      [listenerPromise, handlerPromise, session.prompt(promptString)]);
  assert_equals(handlerEvent, listenerEvent);
  assert_equals(handlerEvent.type, 'contextoverflow');
  assert_equals(handlerEvent.target, session);
}, 'The `contextoverflow` event is fired and `oncontextoverflow` is called when overall usage exceeds the context window');
