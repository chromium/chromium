<?php
$contentType = $_GET["contentType"] ?? null;
// If the magic quotes option is enabled, the charset could be escaped and we
// would fail our test. For example, charset="utf-8" would become charset=\"utf-8\".
if (get_magic_quotes_gpc()) {
    $contentType = stripslashes($contentType);
}
header("Content-Type: $contentType");
?>

id: 77
retry: 300
data: hello

