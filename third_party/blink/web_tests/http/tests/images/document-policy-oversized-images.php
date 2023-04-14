<?php
header("Document-Policy: oversized-images=2.0");
?>
<!DOCTYPE html>
<meta name="fuzzy" content="maxDifference=0-6;totalPixels=0-17">

<head>
  <base href="resources/">
  <script>
    function loaded() {
      if (window.testRunner) {
        testRunner.waitUntilDone();
        testRunner.updateAllLifecyclePhasesAndCompositeThen(() => testRunner.notifyDone());
      }
    }
  </script>
</head>

<!-- Note: give each image a unique URL so that the violation report is actually
sent, instead of being ignored as a duplicate. -->
<body onload=loaded()>
  <div width="600" height="500">
    <img src="green-256x256.jpg?id=1">
    <img src="green-256x256.jpg?id=2" width="100" height="256">
    <img src="green-256x256.jpg?id=3" style="height: 100px; width: 256px">
    <img src="green-256x256.jpg?id=4" width="128" height="128">
    <img src="green-256x256.jpg?id=5" width="50" height="50">
    <img src="green-256x256.jpg?id=6" style="height: 50px; width: 50px">
    <img src="green-256x256.jpg?id=7" style="height: 1cm; width: 1cm">
    <img src="green-256x256.jpg?id=8" style="height: 1cm; width: 1cm; border-radius: 5px; border: 1px solid blue;">
  </div>
</body>
