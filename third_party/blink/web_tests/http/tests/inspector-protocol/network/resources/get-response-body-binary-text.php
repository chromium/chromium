<?php
header("Content-Type: text/plain; charset=utf-8");
// Raw bytes invalid in UTF-8: 0x80 and 0xFE are not valid leading bytes.
echo "\x00\x80\xfe\xff\xc0\x41\xf5\x90";
?>
