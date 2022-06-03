<?php
header("Document-Policy: lossy-images-max-bpp=0.5");
?>
<!DOCTYPE html>
<style>
body {
  font: 10px Ahem;
}
img {
  border: 1px solid blue;
  border-radius: 5px;
}
</style>
<head>
  <base href="resources/">
</head>
<body>
  <div width="440" height="180">
    <img src="Fisher-large.jpg"/>
  </div>
</body>
