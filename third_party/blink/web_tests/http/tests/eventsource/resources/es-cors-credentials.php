<?php
if ($_SERVER["REQUEST_METHOD"] == "OPTIONS")
    die("Got unexpected preflight request");

header("Content-Type: text/event-stream");

$count = intval($_GET["count"]);
$cors = $_GET["cors"] ?? null;

if ($cors)
    header("Access-Control-Allow-Origin: " . $cors);
if ($count == 1 || $count == 2)
    header("Access-Control-Allow-Origin: *");
else if ($count > 2)
    header("Access-Control-Allow-Origin: " . $_SERVER["HTTP_ORIGIN"]);

if ($count == 2 || $count > 3)
    header("Access-Control-Allow-Credentials: true");

if ($_SERVER["HTTP_LAST_EVENT_ID"] != "77")
    echo "id: 77\ndata: DATA1\nretry: 0\n\n";
else
    echo "data: DATA2\n\n";
?>
