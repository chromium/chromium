<?php
$session_id = $_GET['session_id'] ?? 'dbsc-session-id';
header("Content-Type: text/plain");
header("HTTP/1.1 200 OK");
header('Secure-Session-Challenge: "new challenge";id="' . $session_id . '"');
?>