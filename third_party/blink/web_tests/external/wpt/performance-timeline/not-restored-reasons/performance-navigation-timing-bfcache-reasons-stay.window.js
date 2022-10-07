// META: title=RemoteContextHelper navigation using BFCache
// META: script=./test-helper.js
// META: script=/common/dispatcher/dispatcher.js
// META: script=/common/get-host-info.sub.js
// META: script=/common/utils.js
// META: script=/html/browsers/browsing-the-web/remote-context-helper/resources/remote-context-helper.js
// META: script=/websockets/constants.sub.js

'use strict';

// Ensure that notRestoredReasons are only updated after non BFCache navigation.
promise_test(async t => {
  const rcHelper = new RemoteContextHelper();
  // Open a window with noopener so that BFCache will work.
  const rc1 = await rcHelper.addWindow(
      /*config=*/ null, /*options=*/ {features: 'noopener'});
  // Use WebSocket to block BFCache.
  await useWebSocket(rc1);
  const rc1_url = await rc1.executeScript(() => {
    return location.href;
  });
  prepareForBFCache(rc1);

  // Navigate away.
  const rc2 = await rc1.navigateToNew();
  // Navigate back.
  await rc2.historyBack();
  assert_not_bfcached(rc1);

  // Check the reported reasons.
  await assertNotRestoredReasonsEquals(
      rc1,
      /*blocked=*/ true,
      /*url=*/ rc1_url,
      /*src=*/ '',
      /*id=*/ '',
      /*name=*/ '',
      /*reasons=*/['WebSocket'],
      /*children=*/[]);
  prepareForBFCache(rc1);

  await rc1.historyForward();
  await rc2.historyBack();
  // This time no blocking feature is used, so the page is restored
  // from BFCache. Ensure that the previous reasons stay there.
  assert_implements_bfcache(rc1);
  await assertNotRestoredReasonsEquals(
      rc1,
      /*blocked=*/ true,
      /*url=*/ rc1_url,
      /*src=*/ '',
      /*id=*/ '',
      /*name=*/ '',
      /*reasons=*/['WebSocket'],
      /*children=*/[]);
});
