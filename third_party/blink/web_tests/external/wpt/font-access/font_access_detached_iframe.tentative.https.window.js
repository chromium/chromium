//META: script=/resources/testdriver.js
//META: script=/resources/testdriver-vendor.js
//META: script=resources/font-test-utils.js

'use strict';

font_access_test(async t => {
  const iframe = document.createElement('iframe');
  document.body.appendChild(iframe);

  const iframeFontManager = iframe.contentWindow.navigator.fonts;
  const frameDOMException = iframe.contentWindow.DOMException;
  iframe.remove();

  await promise_rejects_dom(
      t, 'InvalidStateError', frameDOMException, iframeFontManager.query());
}, 'query() must return an error when called from a detached iframe.');

font_access_test(async t => {
  const iframe = document.createElement('iframe');
  document.body.appendChild(iframe);

  const iframeFontManager = iframe.contentWindow.navigator.fonts;
  iframeFontManager.query();
  iframe.remove();

  // Call query() in the main frame. This should keep the test running long
  // enough to catch any crash from the query() call in the removed iframe.
  await navigator.fonts.query();
}, 'Detaching iframe while query() settles.');

font_access_test(async t => {
  const iframe = document.createElement('iframe');
  document.body.appendChild(iframe);

  const iframeFonts = await iframe.contentWindow.navigator.fonts.query();
  assert_greater_than_equal(iframeFonts.length, 1, 'Need a least one font');
  const iframeFontData = iframeFonts[0];
  const frameDOMException = iframe.contentWindow.DOMException;
  iframe.remove();

  iframeFontData.blob();
}, 'FontData.blob() should not crash when called from a detached iframe.');
