<?php
  if (!isset($_SERVER['PHP_AUTH_USER'])) {
   header('WWW-Authenticate: Basic realm="WebKit AppCache Test Realm"');
   header('HTTP/1.0 401 Unauthorized');
   echo 'Authentication canceled';
   exit;
  } else {
   header("Content-Type: text/cache-manifest");
   echo "CACHE MANIFEST\n";
   echo "subresource.php\n";
  }
?>
