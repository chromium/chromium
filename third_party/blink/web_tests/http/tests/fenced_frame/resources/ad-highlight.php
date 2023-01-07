<?php
header('Content-Type: text/html');
header('Supports-Loading-Mode: fenced-frame');
?>
<body>
  <script>
    window.internals.setIsAdFrame(window.document);
    if (window.testRunner) {
      testRunner.setHighlightAds();
      testRunner.notifyDone();
    }
  </script>
</body>
