<?php
$csp = $_GET['csp'];
header("content-security-policy: $csp");
?>

<!DOCTYPE html>
<html lang="en">
  <head>
    <meta charset="utf-8">
    <title>Page with restrictive CSP</title>
  </head>
  <body>
  </body>
</html>
