<?php
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: POST');
header('Access-Control-Allow-Headers: header-name');

if (isset($_GET['redirected'])) {
  return;
}
if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
  header('HTTP/1.0 302 Found');
  header('Location: ./cors-preflight-redirect.php?redirected=true');
} else {
  echo 'OK';
}
?>
