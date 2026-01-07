<?php
header("Content-Type: application/json");
header("Access-Control-Allow-Origin: *");

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    header("Set-Cookie: dbsc-cookie=refreshed-cookie-value; Domain=localhost; Path=/; Secure; HttpOnly; SameSite=Lax");
} else {
    header("HTTP/1.1 405 Method Not Allowed");
}
?>