<?php
header("Content-Type: application/json");
header("Access-Control-Allow-Origin: *");

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $session_id = $_GET['session_id'] ?? 'dbsc-session-id';
    $cookie_name = $_GET['cookie_name'] ?? 'dbsc-cookie';
    $domain_prefix = $_GET['domain_prefix'] ?? 'dbsc';
    $domain = $domain_prefix . ".test";
    $refresh_url = "/inspector-protocol/network/resources/dbsc/refresh.php?cookie_name=" . urlencode($cookie_name) . "&domain_prefix=" . urlencode($domain_prefix);

    echo json_encode([
        "session_identifier" => $session_id,
        "refresh_url" => $refresh_url,
        "continue" => true,
        "scope" => [
            "origin" => "https://$domain:8443",
            "include_site" => false,
            "scope_specification" => [
                [
                    "type" => "exclude", 
                    "domain" => $domain,
                    "path" => "/inspector-protocol/network/resources/dbsc/"
                ],
                [
                    "type" => "include", 
                    "domain" => $domain,
                    "path" => "/inspector-protocol/network/resources/dbsc/protected-resource.php"
                ]
            ]
        ],
        "credentials" => [[
            "type" => "cookie",
            "name" => $cookie_name,
            "attributes" => "Domain=$domain; Path=/; Secure; HttpOnly; SameSite=Lax"
        ]],
        "allowed_refresh_initiators" => [$domain]
    ]);
} else {
    header("HTTP/1.1 405 Method Not Allowed");
}
?>