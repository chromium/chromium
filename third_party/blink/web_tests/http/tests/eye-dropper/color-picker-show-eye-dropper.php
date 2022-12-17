<?php
header("Origin-Agent-Cluster: ?0");
?>
<!DOCTYPE html>
<html>
<head>
<meta name="color-scheme" content="light dark">
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
<script src="../../../resources/testdriver.js"></script>
<script src="../../../resources/testdriver-vendor.js"></script>
<script src="/forms-test-resources/picker-common.js"></script>
</head>
<body>
<input type='color' id='color' value='#7EFFC9'>

<p id='description' style='opacity: 0'></p>
<div id='console' style='opacity: 0'></div>

<script type="module">
import {waitUntilEyeDropperShown} from './resources/mock-eyedropperchooser.js';

promise_test(async () => {
  await openPicker(document.getElementById('color'));
  
  internals.pagePopupWindow.focus();
  const popupDocument = internals.pagePopupWindow.document;
  const eyeDropper = popupDocument.querySelector('eye-dropper');
  const eyeDropperRect = eyeDropper.getBoundingClientRect();
  eventSender.mouseMoveTo(eyeDropperRect.left + (eyeDropperRect.width / 2), eyeDropperRect.top + (eyeDropperRect.height / 2));
  eventSender.mouseDown();
  eventSender.mouseUp();
  await waitUntilEyeDropperShown();
}, 'eye dropper is shown when clicked in color chooser');
</script>
</body>
</html>
