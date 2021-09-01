<?php
require_once '../../resources/portabilityLayer.php';

$tmpFile = sys_get_temp_dir() . "/" . "fail_on_update_state";

function setState($newState, $file)
{
    file_put_contents($file, $newState);
}

function getState($file)
{
    if (!file_exists($file)) {
        return "Uninitialized";
    }
    return file_get_contents($file);
}

$command = $_GET['command'];
$state = getState($tmpFile);

header("Expires: Thu, 01 Dec 2003 16:00:00 GMT");
header("Cache-Control: no-cache, must-revalidate");
header("Pragma: no-cache");

if ($command == "reset") {
    unlink($tmpFile);
} else if ($command == "delete") {
    setState("Deleted", $tmpFile);
} else if ($state == "Uninitialized") {
    header("Content-Type: text/cache-manifest");
    print("CACHE MANIFEST\n");
    print("fail-on-update-2.html\n");
    print("NETWORK:\n");
    print("fail-on-update.php?command=\n");
    print("ORIGIN-TRIAL:\n");
    print("AnwB3aSh6U8pmYtO/AzzxELSwk8BRJoj77nUnCq6u3N8LMJKlX/ImydQmXn3SgE0a+8RyowLbV835tXQHJMHuAEAAABQeyJvcmlnaW4iOiAiaHR0cDovLzEyNy4wLjAuMTo4MDAwIiwgImZlYXR1cmUiOiAiQXBwQ2FjaGUiLCAiZXhwaXJ5IjogMTc2MTE3NjE5OH0=\n");
} else if ($state == "Deleted") {
    header('HTTP/1.0 404 Not Found');
}
?>
