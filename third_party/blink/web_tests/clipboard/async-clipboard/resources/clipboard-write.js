'use strict';
async function WritePlainTextAndHTMLToClipboard() {
  await test_driver.set_permission({name: 'clipboard-read'}, 'granted');
  await test_driver.set_permission({name: 'clipboard-write'}, 'granted');
  const formatPlain = 'text/plain';
  const formatHtml = 'text/html';
  const blobPlain = new Blob(['plain text content'], {type: formatPlain});
  const blobHtml = new Blob(['<p>html content</p>'], {type: formatHtml});
  const clipboardItem = new ClipboardItem({
    [formatPlain]: blobPlain,
    [formatHtml]: blobHtml
  });
  await waitForUserActivation();
  await navigator.clipboard.write([clipboardItem]);
}

async function WriteCustomFormatToClipboard() {
  await test_driver.set_permission({name: 'clipboard-read'}, 'granted');
  await test_driver.set_permission({name: 'clipboard-write'}, 'granted');
  const formatPlain = 'text/plain';
  const formatCustom = 'web application/x-custom';
  const blobPlain = new Blob(['plain text content'], {type: formatPlain});
  const blobCustom = new Blob(['custom format data'], {type: 'application/x-custom'});
  const clipboardItem = new ClipboardItem({
    [formatPlain]: blobPlain,
    [formatCustom]: blobCustom
  });
  await waitForUserActivation();
  await navigator.clipboard.write([clipboardItem]);
}
