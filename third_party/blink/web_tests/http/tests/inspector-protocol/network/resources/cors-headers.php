<?php
if (isset($_GET['origin'])) {
  $origin = !empty($_GET['origin']) ? $_GET['origin'] : '*';
  header("Access-Control-Allow-Origin: $origin");
}
$methods = !empty($_GET['methods']) ? $_GET['methods'] : 'OPTIONS';
header("Access-Control-Allow-Methods: $methods");
header('Access-Control-Allow-Headers: content-type');
$REQUEST_METHOD = $_SERVER['REQUEST_METHOD'] ?? '';
if ($REQUEST_METHOD === 'OPTIONS') {
  echo 'replied to options with Access-Control-Allow headers';
  http_response_code(204);
} else {
  echo 'post data: ' . file_get_contents('php://input');
}
?>