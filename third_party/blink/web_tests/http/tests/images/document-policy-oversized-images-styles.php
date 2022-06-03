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
</head>

<body onload=loaded()>
  <!-- Should not trigger violation message. -->
  <img src="resources/green-256x256.jpg?id=1" width="128" height="128" style="border: 10px solid red;">
  <!-- Should generate a violation message each and get replaced with placeholder image. -->
  <img src="resources/green-256x256.jpg?id=2" width="120" height="120" style="border: 10px solid red;">
  <img src="resources/green-256x256.jpg?id=3" width="120" height="120" style="padding: 10px;">
  <img src="resources/green-256x256.jpg?id=4" width="120" height="120" style="border: 10px solid red; padding: 5px;">
  <!-- Note: give each image a unique URL so that the violation report is
    actually sent, instead of being ignored as a duplicate. -->
</body>
