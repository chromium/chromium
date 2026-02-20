<?php
header("Content-Type: application/json");
header("Access-Control-Allow-Origin: *");

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    if (isset($_GET['error_code'])) {
        $error_code = intval($_GET['error_code']);
        header("HTTP/1.1 $error_code Error");
        echo "Error body for refresh $error_code";
        exit;
    }
    $cookie_name = $_GET['cookie_name'] ?? 'dbsc-cookie';
    $domain_prefix = $_GET['domain_prefix'] ?? 'dbsc';
    header("Set-Cookie: $cookie_name=refreshed-cookie-value; Domain=$domain_prefix.test; Path=/; Secure; HttpOnly; SameSite=Lax");
} else {
    header("HTTP/1.1 405 Method Not Allowed");
}
?>