<?php
header("Content-Type: application/json");
header("Access-Control-Allow-Origin: *");
$cookie_name = $_GET['cookie_name'] ?? 'dbsc-cookie';
$domain_prefix = $_GET['domain_prefix'] ?? 'dbsc';
$max_age = $_GET['max_age'] ?? '3600'; // defaults to 1 hour
header("Set-Cookie: $cookie_name=refreshed-cookie-value; Domain=$domain_prefix.test; Path=/; Secure; HttpOnly; SameSite=Lax; Max-Age=$max_age");
?>