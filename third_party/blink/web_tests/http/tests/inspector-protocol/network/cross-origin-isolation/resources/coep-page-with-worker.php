<?php

header("Cross-Origin-Embedder-Policy: require-corp");

?>

<!DOCTYPE html>
<html lang="en">
  <head>
    <title>Page with Cross-Origin-Embedder-Policy</title>
    <meta charset="utf-8">
  </head>
  <body>
    <script>
      const worker = new Worker("script-with-coep-corp.php");
      const worker2 = new Worker("script-with-coep-corp.php?coep");
      const worker3 = new Worker("script-with-coep-corp.php?coep&corp=same-site");
    </script>
  </body>
</html>

