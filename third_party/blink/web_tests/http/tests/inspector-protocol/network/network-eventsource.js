// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests EventSource resource type and content.\n`);

  await dp.Network.enable();

  await session.evaluateAsync(`
    function output(message) {
      if (!self._output)
        self._output = [];
      self._output.push('[page] ' + message);
    }

    function receiveEvent() {
      var callback;
      var promise = new Promise((fulfill) => callback = fulfill);
      var es = new EventSource(${
      JSON.stringify(testRunner.url('./resources/event-stream.asis'))});
      es.onmessage = onMessage;
      es.onerror = onError;
      return promise;
      function onMessage(event) {
        output('got event: ' + event.data);
      }

      function onError() {
        es.close();
        callback();
      }
    }
    Promise.resolve();
  `);

  const requestWillBeSentPromise = dp.Network.onceRequestWillBeSent();
  const responseReceivedPromise = dp.Network.onceResponseReceived();
  const eventSourceMessagePromise = dp.Network.onceEventSourceMessageReceived();
  const loadingFinishedPromise = dp.Network.onceLoadingFinished();
  const loadingFailedPromise = dp.Network.onceLoadingFailed();

  await session.evaluateAsync('receiveEvent()');

  const results = await Promise.race([
    Promise.all([
      requestWillBeSentPromise,
      responseReceivedPromise,
      eventSourceMessagePromise,
      loadingFinishedPromise,
    ]),
    loadingFailedPromise.then(event => {
      testRunner.fail('Resource loading failed: ' + event.params.errorText);
      return null;
    }),
  ]);
  if (!results) {
    return;
  }
  const [requestWillBeSent,
         responseReceived,
         eventSourceMessage,
         loadingFinished,
  ] = results;

  const output = await session.evaluateAsync('JSON.stringify(self._output)');
  for (const line of JSON.parse(output)) {
    testRunner.log(line);
  }
  testRunner.log(requestWillBeSent.params.request.url);
  testRunner.log('resource.type: ' + responseReceived.params.type);
  testRunner.log(eventSourceMessage.params, 'eventSourceMessageReceived: ');

  const {result} = await dp.Network.getResponseBody({
    requestId: loadingFinished.params.requestId,
  });
  testRunner.log('base64Encoded: ' + result.base64Encoded);
  testRunner.log('resource.content after requesting content: ' + result.body);
  testRunner.completeTest();
});
