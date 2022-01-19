<?php
header('Content-Type: text/html');
header('Supports-Loading-Mode: fenced-frame');
?>
<body style="margin: 0px">
  <div style="background: red; width: 20px; height: 20px"></div>
  <script>
    if (window.testRunner)
      testRunner.notifyDone();
  </script>
</body>
