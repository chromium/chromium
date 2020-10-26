<?php
header("Document-Policy: oversized-images=2.0");
?>
<!DOCTYPE html>

<head>
  <script src='/resources/run-after-layout-and-paint.js'></script>
</head>

<body>
  <!--
    Tests that document policy violation on oversized-images is not triggered,
    after images are resized to proper size.

    The initial sizes setting for images all trigger document policy violation.
    It is expected that 4 violation report and corresponding console messages
    generated. (content in -expected.txt)

    After the resize, there should not be any violations triggered.
    It is expected that non of images on page are replaced by placeholders
    after resize. (content in -expected.png)
-->

  <img src="resources/green-256x256.jpg" width="100" height="128">
  <img src="resources/green-256x256.jpg" style="height: 100px; width: 128px">
  <img src="resources/green-256x256.jpg" width="100" height="100">
  <img src="resources/green-256x256.jpg" style="height: 100px; width: 100px">

  <script>
    runAfterLayoutAndPaint(function() {
      var images = document.getElementsByTagName('img');
      for (var i = 0; i < images.length; i++) {
        var image = images[i];
        if (image.hasAttribute('width') || image.hasAttribute('height')) {
          image.width = "150";
          image.height = "150";
        } else {
          image.style.width = "150px";
          image.style.height = "150px";
        }
      }
    }, true);
  </script>
</body>

</html>