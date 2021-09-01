<?php
  if (!isset($_SERVER['PHP_AUTH_USER'])) {
   header('WWW-Authenticate: Basic realm="WebKit AppCache Test Realm"');
   header('HTTP/1.0 401 Unauthorized');
   echo 'Authentication canceled';
   exit;
  } else {
   echo "<html manifest='manifest.php'>\n";
   echo "<p>Subframe, should disappear.</p>\n";
   echo "<script>\n";
   echo "    applicationCache.oncached = parent.success;\n";
   echo "    applicationCache.onnoupdate = parent.success;\n";
   echo "</script>\n";
   echo "</html>\n";
  }
?>
