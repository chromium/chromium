// META: script=/resources/testdriver.js
// META: script=/resources/testdriver-vendor.js

"use strict";

promise_test(async (t) => {
  const permissionDescriptorNames = [
    "accelerometer",
    "gyroscope",
    "magnetometer",
  ];
  await Promise.all(
    permissionDescriptorNames.map((name) =>
      test_driver.set_permission({ name }, "granted"),
    ),
  );

  const iframe = document.createElement("iframe");
  document.body.appendChild(iframe);
  t.add_cleanup(() => {
    iframe.remove();
  });

  const iframeWindow = iframe.contentWindow;
  const IframeDeviceOrientationEvent = iframeWindow.DeviceOrientationEvent;

  const permission = await IframeDeviceOrientationEvent.requestPermission();
  assert_equals(permission, "granted");
}, 'requestPermission() resolves to "granted" when the document is fully active');

promise_test(async (t) => {
  const iframe = document.createElement("iframe");
  document.body.appendChild(iframe);

  const iframeWindow = iframe.contentWindow;
  const IframeDeviceOrientationEvent = iframeWindow.DeviceOrientationEvent;
  const IframeDOMException = iframeWindow.DOMException;

  iframe.remove();

  await promise_rejects_dom(
    t,
    "InvalidStateError",
    IframeDOMException,
    IframeDeviceOrientationEvent.requestPermission(),
  );
}, "requestPermission() rejects with InvalidStateError when the document is not fully active");
