// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} =
      await testRunner.startBlank(`Tests disabling cache from inspector.\n`);

  await dp.Page.enable();
  await dp.Network.enable();
  await dp.Runtime.enable();

  const failures = [];
  dp.Runtime.onExceptionThrown(event => {
    failures.push(`Uncaught exception: ${
        event.params.exceptionDetails?.text ||
        JSON.stringify(event.params.exceptionDetails)}`);
  });
  dp.Network.onLoadingFailed(event => {
    failures.push(`Network loadingFailed (${event.params.requestId}): ${
        event.params.errorText}`);
  });

  await session.navigate('./resources/random-script-page.html');

  async function loadScriptAndGetStatusAndContent() {
    let targetRequestId = null;
    const responseStatuses = new Map();
    const finishedRequests = new Set();
    let resolveDone;
    const donePromise = new Promise(resolve => {
      resolveDone = resolve;
    });

    function maybeResolve() {
      if (targetRequestId && responseStatuses.has(targetRequestId) &&
          finishedRequests.has(targetRequestId)) {
        resolveDone();
      }
    }

    function onRequestWillBeSent(event) {
      if (event.params.request.url.endsWith('/random-script.php')) {
        targetRequestId = event.params.requestId;
        maybeResolve();
      }
    }
    function onResponseReceived(event) {
      responseStatuses.set(event.params.requestId,
                           event.params.response.status);
      maybeResolve();
    }
    function onLoadingFinished(event) {
      finishedRequests.add(event.params.requestId);
      maybeResolve();
    }

    dp.Network.onRequestWillBeSent(onRequestWillBeSent);
    dp.Network.onResponseReceived(onResponseReceived);
    dp.Network.onLoadingFinished(onLoadingFinished);
    const consolePromise = dp.Runtime.onceConsoleAPICalled(
        event => event.params.args[0]?.value === 'Done.');

    try {
      await session.evaluate('scheduleScriptLoad()');
      await Promise.all([donePromise, consolePromise]);
    } finally {
      dp.Network.offRequestWillBeSent(onRequestWillBeSent);
      dp.Network.offResponseReceived(onResponseReceived);
      dp.Network.offLoadingFinished(onLoadingFinished);
    }

    const bodyResponse =
        await dp.Network.getResponseBody({requestId: targetRequestId});
    return {
      status: responseStatuses.get(targetRequestId),
      content: bodyResponse.result?.body,
    };
  }

  async function reloadPage() {
    const loadPromise = dp.Page.onceLoadEventFired();
    await dp.Page.reload();
    await loadPromise;
    testRunner.log('Page reloaded.');
  }

  const {status: status1, content: content1} =
      await loadScriptAndGetStatusAndContent();
  await reloadPage();

  const {status: status2} = await loadScriptAndGetStatusAndContent();
  await dp.Network.setCacheDisabled({cacheDisabled: true});
  await reloadPage();

  const {status: status3, content: content3} =
      await loadScriptAndGetStatusAndContent();

  if (failures.length > 0) {
    testRunner.fail(failures.join('\n'));
    return;
  }
  if (status1 !== 200) {
    testRunner.fail(`First script load should be 200, got ${status1}`);
    return;
  }
  if (status2 !== 304) {
    testRunner.fail(
        `Second script load should be from cache (expected 304, got ${
            status2})`);
    return;
  }
  if (!content1 || !content3 || !content1.includes('var randomValue = ') ||
      !content3.includes('var randomValue = ') || content1 === content3) {
    testRunner.fail('First and third scripts should differ.');
    return;
  }
  if (status3 !== 200) {
    testRunner.fail(
        `Third script load should be from network (expected 200, got ${
            status3})`);
    return;
  }

  await dp.Network.setCacheDisabled({cacheDisabled: false});
  testRunner.completeTest();
});
