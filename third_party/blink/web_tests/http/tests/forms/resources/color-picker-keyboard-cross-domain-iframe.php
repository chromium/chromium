<?php
header("Origin-Agent-Cluster: ?0");
?>
<html>
  <input type="color" id="color" value="#126465">

  <script>
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
  </script>
</html>
