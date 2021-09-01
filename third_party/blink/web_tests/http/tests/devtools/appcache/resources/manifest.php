<?php
    header("Expires: Thu, 01 Dec 2003 16:00:00 GMT");
    header("Cache-Control: no-cache, must-revalidate");
    header("Pragma: no-cache");
    header("Content-Type: text/cache-manifest");

    $manifestId = $_GET["manifestId"];

    echo("CACHE MANIFEST\n");
    echo("# " . $manifestId . "\n");
    echo("page-with-manifest.php?manifestId=" . $manifestId . "\n");

    if ($manifestId == "with-non-existing-file")
        echo("non-existing-file\n");
?>
