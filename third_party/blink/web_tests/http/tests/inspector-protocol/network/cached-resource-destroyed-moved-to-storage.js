// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests content is moved from cached resource to resource agent's data storage when cached resource is destroyed.\n`);

  await dp.Runtime.enable();
  await dp.Network.enable();

  await dp.Runtime.evaluate({
    expression: `
      var image;
      function loadFirstImage() {
          image = new Image();
          image.onload = firstImageLoaded;
          document.body.appendChild(image);
          image.src = "/devtools/network/resources/resource.php?type=image&random=1&size=400";
      }
      function firstImageLoaded() {
          console.log("Done1.");
      }
      function loadSecondImage() {
          image.onload = secondImageLoaded;
          image.src = "/devtools/network/resources/resource.php?type=image&random=1&size=200";
      }
      function secondImageLoaded() {
          console.log("Done2.");
      }
    `,
    objectGroup: 'console',
  });

  const firstRequestWillBeSentPromise = dp.Network.onceRequestWillBeSent();
  const firstLoadingFinishedPromise = dp.Network.onceLoadingFinished();
  const firstConsolePromise = dp.Runtime.onceConsoleAPICalled(
      event => event.params.args[0]?.value === 'Done1.');

  await dp.Runtime.evaluate({
    expression: 'loadFirstImage()',
    objectGroup: 'console',
  });

  const firstRequest = (await firstRequestWillBeSentPromise).params;
  await firstLoadingFinishedPromise;
  await firstConsolePromise;

  const firstBodyResponse =
      await dp.Network.getResponseBody({requestId: firstRequest.requestId});
  const content = firstBodyResponse.result.body;

  testRunner.log(firstRequest.request.url);
  testRunner.log('request.type: ' + firstRequest.type.toLowerCase());
  testRunner.log('request.content.length after requesting content: ' +
                 content.length);
  if (!(content.length > 0)) {
    testRunner.fail('No content before destroying CachedResource.');
    return;
  }
  testRunner.log('Releasing cached resource.');

  const secondLoadingFinishedPromise = dp.Network.onceLoadingFinished();
  const secondConsolePromise = dp.Runtime.onceConsoleAPICalled(
      event => event.params.args[0]?.value === 'Done2.');

  await dp.Runtime.evaluate({
    expression: 'loadSecondImage()',
    objectGroup: 'console',
  });

  await secondLoadingFinishedPromise;
  await secondConsolePromise;

  await dp.Network.setCacheDisabled({cacheDisabled: true});
  await dp.Network.setCacheDisabled({cacheDisabled: false});
  for (let i = 0; i < 3; ++i) {
    await session.evaluateAsync(
        `new Promise(resolve => GCController.asyncCollectAll(resolve))`);
  }

  const secondBodyResponse =
      await dp.Network.getResponseBody({requestId: firstRequest.requestId});
  const contentAfterEviction = secondBodyResponse.result.body;

  testRunner.log('request.content.length after requesting content: ' +
                 contentAfterEviction.length);
  if (contentAfterEviction !== content ||
      secondBodyResponse.result.base64Encoded !==
          firstBodyResponse.result.base64Encoded) {
    testRunner.fail('Content changed after cached resource was destroyed');
    return;
  }

  testRunner.completeTest();
});
