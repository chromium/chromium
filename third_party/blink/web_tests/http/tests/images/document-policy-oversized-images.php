<?php
header("Document-Policy: oversized-images=2.0");
?>
<!DOCTYPE html>
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
<body onload=loaded()>
  <div width="600" height="500">
    <img src="green-256x256.jpg">
    <img src="green-256x256.jpg" width="100" height="256">
    <img src="green-256x256.jpg" style="height: 100px; width: 256px">
    <img src="green-256x256.jpg" width="128" height="128" >
    <img src="green-256x256.jpg" width="50" height="50">
    <img src="green-256x256.jpg" style="height: 50px; width: 50px">
    <img src="green-256x256.jpg" style="height: 1cm; width: 1cm">
    <img src="green-256x256.jpg" style="height: 1cm; width: 1cm; border-radius: 5px; border: 1px solid blue;">
  </div>
</body>
