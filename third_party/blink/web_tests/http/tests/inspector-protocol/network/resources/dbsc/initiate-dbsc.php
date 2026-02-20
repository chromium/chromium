<?php
$session_id = $_GET['session_id'] ?? 'dbsc-session-id';
$cookie_name = $_GET['cookie_name'] ?? 'dbsc-cookie';
$domain_prefix = $_GET['domain_prefix'] ?? 'dbsc';
$error_param = isset($_GET['error_code']) ? "&error_code=" . urlencode($_GET['error_code']) : "";
$refresh_error_param = isset($_GET['refresh_error_code']) ? "&refresh_error_code=" . urlencode($_GET['refresh_error_code']) : "";
$reg_path = $_GET['reg_url'] ?? ("/inspector-protocol/network/resources/dbsc/registration.php?session_id=" . urlencode($session_id) . "&cookie_name=" . urlencode($cookie_name). "&domain_prefix=" . urlencode($domain_prefix) . $error_param . $refresh_error_param);
header("Secure-Session-Registration: (ES256);path=\"$reg_path\";challenge=\"test-challenge\";authorization=\"test-auth\"");
header("Content-Type: text/html");
?>