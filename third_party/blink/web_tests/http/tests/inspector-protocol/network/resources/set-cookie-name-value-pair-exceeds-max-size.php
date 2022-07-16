<?php
// Set a cookie that's 4097 characters, which is one more than the maximum
// for name + value pairs (as specified by RFC6265bis).
header("set-cookie: " . str_repeat('a', 4097));
?>
