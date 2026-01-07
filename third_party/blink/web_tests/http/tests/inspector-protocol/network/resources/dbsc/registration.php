<?php
header("Content-Type: application/json");
header("Access-Control-Allow-Origin: *");

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $session_id = "dbsc-session-id";
    header("Set-Cookie: dbsc-registration-complete=true");
    echo json_encode([
        "session_identifier" => $session_id,
        "refresh_url" => "/inspector-protocol/network/resources/dbsc/refresh.php",
        "continue" => true,
        "scope" => [
            "origin" => "http://localhost:8080",
            "include_site" => false,
            "scope_specification" => []
        ],
        "credentials" => [[
            "type" => "cookie",
            "name" => "dbsc-cookie",
            "attributes" => "Domain=localhost; Path=/; Secure; HttpOnly; SameSite=Lax"
        ]],
        "allowed_refresh_initiators" => ["localhost"]
    ]);
} else {
    header("HTTP/1.1 405 Method Not Allowed");
}
?>
