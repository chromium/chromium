<?php
header('Content-Type: text/html');
header('Supports-Loading-Mode: fenced-frame');
?>
<body style="margin: 0px">
  <div id="placeholder" style="background: red; width: 20px; height: 20px"></div>
  <script>
    document.addEventListener("visibilitychange", () => {
      if ( document.visibilityState != "visible" ) {
        document.getElementById('placeholder').style.background = 'green';
      }
      if (window.testRunner)
        testRunner.setMainWindowHidden(false);
        testRunner.notifyDone();
    });
    if (window.testRunner)
      testRunner.setMainWindowHidden(true);
  </script>
</body>
