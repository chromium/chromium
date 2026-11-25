// META: script=/resources/testdriver.js
// META: script=/resources/testdriver-vendor.js

promise_test(async t => {
  const iframe = document.createElement("iframe");
  t.add_cleanup(() => iframe.remove());
  document.body.appendChild(iframe);

  await new Promise(resolve => {
    iframe.addEventListener("load", resolve, { once: true });
    iframe.src = "/screen-wake-lock/resources/page1.html";
  });

  const { wakeLock } = iframe.contentWindow.navigator;
  const frameDOMException = iframe.contentWindow.DOMException;

  await test_driver.set_permission(
      { name: "screen-wake-lock" }, "granted", iframe.contentWindow);
  await test_driver.bless("screen wake lock", null, iframe.contentWindow);
  const promise = wakeLock.request("screen");

  iframe.remove();

  await promise_rejects_dom(t, "AbortError", frameDOMException, promise, "Document stopped being fully active");
}, "navigator.wakeLock.request() aborts if the document becomes not active due to iframe removal");
