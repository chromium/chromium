// META: title=NativeIO API: Do not crash in detached iframes.
// META: global=window

function add_iframe(test, src, sandbox) {
  const iframe = document.createElement("iframe");
  document.body.appendChild(iframe);
  return iframe;
}

promise_test(async testCase => {
  const iframe = add_iframe();
  const iframe_sfa = iframe.contentWindow.storageFoundation;
  const frameDOMException = iframe.contentWindow.DOMException;
  iframe.remove();

  await promise_rejects_dom(
    testCase, 'InvalidStateError', frameDOMException,
    iframe_sfa.getAll());
  await promise_rejects_dom(
      testCase, 'InvalidStateError', frameDOMException,
      iframe_sfa.open('test_file'));
  await promise_rejects_dom(
      testCase, 'InvalidStateError', frameDOMException,
      iframe_sfa.rename('test_file', 'test'));
  await promise_rejects_dom(
      testCase, 'InvalidStateError', frameDOMException,
      iframe_sfa.delete('test'));
  await promise_rejects_dom(
      testCase, 'InvalidStateError', frameDOMException,
      iframe_sfa.requestCapacity(10));
  await promise_rejects_dom(
      testCase, 'InvalidStateError', frameDOMException,
      iframe_sfa.releaseCapacity(10));
  await promise_rejects_dom(
      testCase, 'InvalidStateError', frameDOMException,
      iframe_sfa.getRemainingCapacity());
}, 'storageFoundation must return an error when called from detached iframes.');
