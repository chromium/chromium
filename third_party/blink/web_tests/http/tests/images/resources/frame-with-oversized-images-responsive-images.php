<?php
header("Document-Policy: oversized-images=2.0");
?>
<!DOCTYPE html>
<body>
  <img srcset="green-256x256.jpg 256w" sizes="100px" width="127" height="127">
  <img srcset="green-256x256.jpg 256w" sizes="100px" width="128" height="128">
  <img srcset="green-256x256.jpg 256w" sizes="100px" width="129" height="129">
</body>
