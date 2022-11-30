<?php
header('reporting-endpoints: main-endpoint="https://reports.example/main", default="https://reports.example/default", csp="https://localhost/reporting-endpoint"');
header("content-security-policy-report-only: script-src 'none'; object-src 'none'; report-to csp");
?>

<!DOCTYPE html>
<html lang="en">
  <head>
    <meta charset="utf-8">
    <title>Page which generates CSP-related report</title>
  </head>
  <body>
    <script>
      console.log('hello');
    </script>
  </body>
</html>
