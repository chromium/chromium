<?php
$lastEventId = $_SERVER['HTTP_LAST_EVENT_ID'] ?? null;

header("Location: /eventsource/resources/echo-url.php?id=" . $lastEventId);
header("Content-Type: text/event-stream");
?>
