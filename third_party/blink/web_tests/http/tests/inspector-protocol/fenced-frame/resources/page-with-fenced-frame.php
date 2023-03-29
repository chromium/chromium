<?php
header('Content-Type: text/html');
header('Supports-Loading-Mode: fenced-frame');
?>
<!DOCTYPE html>
<body>
  <fencedframe></fencedframe>
  <script>
  const url = new URL("page-with-title.php", location.href);
  document.querySelector("fencedframe").config = new FencedFrameConfig(url);
</script>
</body>
