<?php
$origin = !empty($_GET['origin']) ? $_GET['origin'] : '*';
if (isset($_GET['origin'])) {
  header("Access-Control-Allow-Origin: $origin");
}
$methods = !empty($_GET['methods']) ? $_GET['methods'] : 'OPTIONS';
header("Access-Control-Allow-Methods: $methods");
header('Access-Control-Allow-Headers: content-type');
if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
  echo 'replied to options with Access-Control-Allow headers';
  http_response_code(204);
} else {
  echo 'post data: ' . file_get_contents('php://input');
}
?>
