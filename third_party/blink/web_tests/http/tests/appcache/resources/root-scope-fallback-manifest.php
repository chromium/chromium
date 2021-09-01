<?php
require_once '../../resources/portabilityLayer.php';

header("Expires: Thu, 01 Dec 2003 16:00:00 GMT");
header("Cache-Control: no-cache, must-revalidate");
header("Pragma: no-cache");
header("Content-Type: text/cache-manifest");
header("X-AppCache-Allowed: /");

print("CACHE MANIFEST\n");
print("FALLBACK:\n");
print("/resources/network-simulator.php? simple.txt\n");
print("fallback-redirect simple.txt\n");
print("does-not-exist simple.txt\n");
print("ORIGIN-TRIAL:\n");
print("AnwB3aSh6U8pmYtO/AzzxELSwk8BRJoj77nUnCq6u3N8LMJKlX/ImydQmXn3SgE0a+8RyowLbV835tXQHJMHuAEAAABQeyJvcmlnaW4iOiAiaHR0cDovLzEyNy4wLjAuMTo4MDAwIiwgImZlYXR1cmUiOiAiQXBwQ2FjaGUiLCAiZXhwaXJ5IjogMTc2MTE3NjE5OH0=\n");
?>
