<?php
header("Document-Policy: oversized-images=2.0");
?>
<!DOCTYPE html>
<script>
  function loaded() {
    if (window.testRunner) {
      testRunner.waitUntilDone();
      testRunner.updateAllLifecyclePhasesAndCompositeThen(() => testRunner.notifyDone());
    }
  }
</script>

<body onload=loaded()>
  <!-- Note: give each image a unique URL so that the violation report is
    actually sent, instead of being ignored as a duplicate. -->
  <img srcset="resources/green-256x256.jpg?id=1 256w" sizes="100px" width="127" height="127">
  <img srcset="resources/green-256x256.jpg?id=2 256w" sizes="100px" width="128" height="128">
  <img srcset="resources/green-256x256.jpg?id=3 256w" sizes="100px" width="129" height="129">
</body>
