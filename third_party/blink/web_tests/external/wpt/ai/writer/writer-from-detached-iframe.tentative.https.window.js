// META: title=Writer Detached Iframe
// META: script=../resources/util.js
// META: timeout=long

'use strict';

promise_test(async (t) => {
  const iframe = document.body.appendChild(document.createElement('iframe'));
  iframe.contentWindow.Writer.create();
  iframe.remove();
}, 'Detaching iframe during Writer.create() should not leak memory');

promise_test(async (t) => {
  const iframe = document.body.appendChild(document.createElement('iframe'));
  const iframeWindow = iframe.contentWindow;
  const iframeDOMException = iframeWindow.DOMException;
  const iframeWriter = iframeWindow.Writer;
  iframe.remove();

  await promise_rejects_dom(
      t, 'InvalidStateError', iframeDOMException, iframeWriter.create());
}, 'Writer.create() fails on a detached iframe.');

promise_test(async (t) => {
  const iframe = document.body.appendChild(document.createElement('iframe'));
  const iframeDOMException = iframe.contentWindow.DOMException;
  const writer = await iframe.contentWindow.Writer.create();
  iframe.remove();

  await promise_rejects_dom(
      t, 'InvalidStateError', iframeDOMException, writer.write('hello'));
}, 'Writer.write() fails on a detached iframe.');

promise_test(async (t) => {
  const iframe = document.body.appendChild(document.createElement('iframe'));
  const iframeWindow = iframe.contentWindow;
  const iframeDOMException = iframeWindow.DOMException;
  const writer = await iframeWindow.Writer.create();
  iframe.remove();

  assert_throws_dom(
      'InvalidStateError', iframeDOMException, () => writer.writeStreaming('hello'));
}, 'Writer.writeStreaming() fails on a detached iframe.');

promise_test(async (t) => {
  const iframe = document.body.appendChild(document.createElement('iframe'));
  const writer = await iframe.contentWindow.Writer.create();
  writer.write('hello');
  iframe.remove();
}, 'Detaching iframe during Writer.write() should not leak memory');
