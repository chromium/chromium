<?php
header("Content-Type: application/json");
header("Access-Control-Allow-Origin: *");

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $cookie_name = $_GET['cookie_name'] ?? 'dbsc-cookie';
    $domain_prefix = $_GET['domain_prefix'] ?? 'dbsc';
    header("Set-Cookie: $cookie_name=refreshed-cookie-value; Domain=$domain_prefix.test; Path=/; Secure; HttpOnly; SameSite=Lax");
} else {
    header("HTTP/1.1 405 Method Not Allowed");
}
?>