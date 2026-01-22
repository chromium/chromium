<?php
$session_id = $_GET['session_id'] ?? 'dbsc-session-id';
$cookie_name = $_GET['cookie_name'] ?? 'dbsc-cookie';
$domain_prefix = $_GET['domain_prefix'] ?? 'dbsc';
$reg_path = "/inspector-protocol/network/resources/dbsc/registration.php?session_id=" . urlencode($session_id) . "&cookie_name=" . urlencode($cookie_name). "&domain_prefix=" . urlencode($domain_prefix);
header("Secure-Session-Registration: (ES256);path=\"$reg_path\";challenge=\"test-challenge\";authorization=\"test-auth\"");
header("Content-Type: text/html");
?>