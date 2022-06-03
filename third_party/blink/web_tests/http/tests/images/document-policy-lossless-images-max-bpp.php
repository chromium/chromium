<?php
header("Document-Policy: lossless-images-max-bpp=1.0");
?>
<!DOCTYPE html>
<style>
body {
  font: 10px Ahem;
}
</style>
<body width="700" height="500">
  <img src="resources/Fisher-large.jpg" width="200"/>
  <img src="resources/Fisher-small.jpg" width="200"/>
  <img src="resources/pass-all.png" width="200"/>
  <img src="resources/fail-strict.png" width="200"/>
  <img src="resources/fail-all.png" width="200"/>
</body>
