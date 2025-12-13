<?php
header("Access-Control-Allow-Origin: *");
$headers = explode(":", $_GET['headers']);
foreach ($headers as $header) {
    echo $header . ": " . (isset($_SERVER[$header]) ? $_SERVER[$header] : "<not set>") . "\n";
}
?>
