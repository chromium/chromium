<?php
$contentType = $_GET["contentType"] ?? null;
header("Content-Type: $contentType");
echo "id: 77\n";
echo "retry: 300\n";
echo "data: hello\n\n";
?>
