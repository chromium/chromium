<?php
header("Document-Policy: oversized-images=2.0");
?>
<!DOCTYPE html>

<head>
  <script>
    function loaded() {
      if (window.testRunner) {
        testRunner.waitUntilDone();
        testRunner.updateAllLifecyclePhasesAndCompositeThen(() => testRunner.notifyDone());
      }
    }
  </script>
  <style>
    body {
      margin: 0;
    }
  </style>
</head>

<body onload=loaded()>
  <!-- Note: give each image a unique URL so that the violation report is actually
    sent, instead of being ignored as a duplicate. -->
  <img src="resources/green-256x256.jpg?id=1" width="100" height="100">
  <img src="resources/green-256x256.jpg?id=2" style="width: 100px; height: 100px">

  <script>
    document.body.offsetTop;
  </script>
</body>
