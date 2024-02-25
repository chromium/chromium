<?php
// Use a tab character that gets treated as white space and removed, resulting
// in an empty cookie which is invalid.
header("set-cookie: \x09");
?>
