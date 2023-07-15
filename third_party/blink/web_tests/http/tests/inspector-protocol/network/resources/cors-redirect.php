<?php
// Entry point and will redirect to redirect2.php then final.html.
header('HTTP/1.1 307');
header('Cache-Control: no-cache, must-revalidate');
header('Pragma: no-cache');
header("Access-Control-Allow-Origin: *");
header("Access-Control-Allow-Methods: GET");
header('Access-Control-Allow-Headers: content-type');
$redirect = $_GET['redirect'] ?? null;
header("Location: cors-headers.php?origin=http%3A%2F%2F127.0.0.1");
?>
