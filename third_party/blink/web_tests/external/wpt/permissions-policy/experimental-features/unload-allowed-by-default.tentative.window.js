// META: title='unload' Policy : allowed by default
// META: script=/common/dispatcher/dispatcher.js
// META: script=/common/utils.js
// META: script=/resources/testharness.js
// META: script=/resources/testharnessreport.js
// META: script=/html/browsers/browsing-the-web/remote-context-helper/resources/remote-context-helper.js
// META: script=./resources/unload-helper.js

'use strict';

// Check that unload is allowed by policy in main frame and subframe by default.
promise_test(async t => {
  const rcHelper =
      new RemoteContextHelper({scripts: ['./resources/unload-helper.js']});
  // In the same browsing context group to ensure BFCache is not used.
  const main = await rcHelper.addWindow();
  const subframe = await main.addIframe();
  await assertWindowRunsUnload(subframe, 'subframe', {shouldRunUnload: true});
  await assertWindowRunsUnload(main, 'main', {shouldRunUnload: true});
});
