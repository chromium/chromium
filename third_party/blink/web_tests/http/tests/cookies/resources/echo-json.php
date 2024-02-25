<?php
$request_origin_value = $_SERVER["HTTP_ORIGIN"] ?? null;
header("Content-Type: application/json");
header("Access-Control-Allow-Credentials: true");
header("Access-Control-Allow-External: true");
header("Access-Control-Allow-Origin: " . $request_origin_value);

echo json_encode($_COOKIE);
?>
