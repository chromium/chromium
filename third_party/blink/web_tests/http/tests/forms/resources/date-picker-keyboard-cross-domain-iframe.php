<?php
header("Origin-Agent-Cluster: ?0");
?>
<html>
  <input type="date" id="date" value="2019-12-12">

  <script>
    // This document can start running script before the cross-process parent
    // has laid out the <iframe> that contains it. Until then this frame is
    // treated as display:none, so nothing in it is focusable and the key press
    // below would not open the picker. Animation frame callbacks only run once
    // this frame is rendered, so wait for one before interacting.
    requestAnimationFrame(() => {
      try {
        let input_date = document.getElementById('date');

        input_date.focus();

        eventSender.keyDown(" ");

        // Advance date to the next day
        eventSender.keyDown("ArrowRight");

        // Make the chosen value apply synchronously so we don't need to insert
        // an artificial delay in the test
        internals.pagePopupWindow.CalendarPicker.commitDelayMs = 0;

        // Close the popup and commit the value
        eventSender.keyDown("Enter");

        window.top.postMessage(`Date result: ${input_date.value}`, "*");
      } catch (e) {
        // Report the error so that the test fails instead of timing out.
        window.top.postMessage(`Date result: ${e}`, "*");
      }
    });
  </script>
</html>
