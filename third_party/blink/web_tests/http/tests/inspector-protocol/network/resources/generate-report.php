<?php
header('report-to: {"group":"csp","max_age":86400,"include_subdomains":true,"endpoints":[{"url":"https://localhost/reporting-endpoint"}]}');
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
