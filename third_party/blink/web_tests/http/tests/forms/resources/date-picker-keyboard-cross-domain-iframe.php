<?php
header("Origin-Agent-Cluster: ?0");
?>
<html>
  <input type="date" id="date" value="2019-12-12">

  <script>
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
  </script>
</html>
