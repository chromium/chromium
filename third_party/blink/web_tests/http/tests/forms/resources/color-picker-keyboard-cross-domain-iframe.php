<?php
header("Origin-Agent-Cluster: ?0");
?>
<html>
  <input type="color" id="color" value="#126465">

  <script>
    // This document can start running script before the cross-process parent
    // has laid out the <iframe> that contains it. Until then this frame is
    // treated as display:none, so nothing in it is focusable and the key press
    // below would not open the picker. Animation frame callbacks only run once
    // this frame is rendered, so wait for one before interacting.
    requestAnimationFrame(() => {
      try {
        let colorElement = document.getElementById('color');

        // Open color picker.
        colorElement.focus();
        eventSender.keyDown(" ");

        internals.pagePopupWindow.focus();
        const popupDocument = internals.pagePopupWindow.document;
        const rValueContainer = popupDocument.getElementById('rValueContainer');
        rValueContainer.focus();
        eventSender.keyDown("Backspace");
        eventSender.keyDown("Backspace");
        eventSender.keyDown("5");
        eventSender.keyDown("0");

        window.top.postMessage(`Color result: ${rValueContainer.value}`, "*");
      } catch (e) {
        // Report the error so that the test fails instead of timing out.
        window.top.postMessage(`Color result: ${e}`, "*");
      }
    });
  </script>
</html>
