<?php
header('Content-Type: text/html');
header('Supports-Loading-Mode: fenced-frame');
?>

<html>
<script src="/resources/get-host-info.js"></script>

<body style="background: red;">
<!-- This page is meant to run inside of a fenced frame -->
<script>
  const remote_origin = get_host_info().HTTP_REMOTE_ORIGIN;
  const remote_url =
      new URL("/fenced_frame/resources/send-coop-headers.php", remote_origin);

  // This navigation will cause a browsing context group swap in the fenced
  // frame.
  location.href = remote_url;
</script>
</body>
</html>
