<?php
header("Document-Policy: oversized-images=2.0");
?>
<!DOCTYPE html>

<!--
  Images should be replaced by placeholders if they are considered
  oversized, i.e. having
  image_size / (container_size * pixel_ratio) > threshold.
  Threshold is set by the document policy header(2.0 in this test).
-->

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
  <!-- Note: give each image a unique URL so that the violation report is
    actually sent, instead of being ignored as a duplicate. -->
  <div style="display: inline-block;">
    <p style="width: 100%;">
      Following cases are for device pixel ratio = 1.0.<br>
      Image with size < 128 should all be replaced with placeholders.
    </p>
    <table>
      <tr>
        <th>size</th>
        <th>127</th>
        <th>128</th>
        <th>129</th>
      </tr>
      <tr>
        <td></td>
        <td><img src="resources/green-256x256.jpg?id=1" width="127" height="127"></td>
        <td><img src="resources/green-256x256.jpg?id=2" width="128" height="128"></td>
        <td><img src="resources/green-256x256.jpg?id=3" width="129" height="129"></td>
      </tr>
    </table>
  </div>
  <div style="display: inline-block;">
    <p style="width: 100%;">
      Following cases are for device pixel ratio = 2.0.<br>
      Image with size < 64 should all be replaced with placeholders.
    </p>
    <table>
      <tr>
        <th>size</th>
        <th>63</th>
        <th>64</th>
        <th>65</th>
      </tr>
      <tr>
        <td></td>
        <td><img src="resources/green-256x256.jpg?id=4" width="63" height="63"></td>
        <td><img src="resources/green-256x256.jpg?id=5" width="64" height="64"></td>
        <td><img src="resources/green-256x256.jpg?id=6" width="65" height="65"></td>
      </tr>
    </table>
  </div>
</body>
