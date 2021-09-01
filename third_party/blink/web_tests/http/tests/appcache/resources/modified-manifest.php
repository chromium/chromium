<?php
require_once '../../resources/portabilityLayer.php';

# This manifest references itself with a command to bump a counter, so
# that a subsequent fetch of the same manifest URL returns a file that
# is not byte-for-byte identical, which will cause caching to fail.

$tmpFile = sys_get_temp_dir() . "/" . "appcache_modified_manifest_counter";

function getCount($file)
{
    if (!file_exists($file)) {
        file_put_contents($file, "0");
        return "0";
    }
    return file_get_contents($file);
}

function stepCounter($file)
{
    if (file_exists($file)) {
        $value = getCount($file);
        file_put_contents($file, ++$value);
    }
}

if ("step" == $_GET['command'])
    stepCounter($tmpFile);

header("Expires: Thu, 01 Dec 2003 16:00:00 GMT");
header("Cache-Control: no-cache, must-revalidate");
header("Pragma: no-cache");
header("Content-Type: text/cache-manifest");

print("CACHE MANIFEST\n");
print("# version " . getCount($tmpFile) . "\n");
print("CACHE:\n");
print("modified-manifest.php?command=step\n");
print("ORIGIN-TRIAL:\n");
print("AnwB3aSh6U8pmYtO/AzzxELSwk8BRJoj77nUnCq6u3N8LMJKlX/ImydQmXn3SgE0a+8RyowLbV835tXQHJMHuAEAAABQeyJvcmlnaW4iOiAiaHR0cDovLzEyNy4wLjAuMTo4MDAwIiwgImZlYXR1cmUiOiAiQXBwQ2FjaGUiLCAiZXhwaXJ5IjogMTc2MTE3NjE5OH0=\n");
?>
