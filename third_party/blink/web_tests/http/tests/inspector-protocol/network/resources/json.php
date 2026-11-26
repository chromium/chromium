<?php
header("Content-Type: " . (isset($_GET["type"]) ? $_GET["type"] : "application/json"));
?>
{"number": "42"}
