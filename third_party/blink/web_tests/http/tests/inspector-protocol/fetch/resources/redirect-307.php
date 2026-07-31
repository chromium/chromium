<?php
// Redirects to the URL given in the query string with a 307, which preserves
// the request method and body.
header('Location: ' . $_SERVER['QUERY_STRING'], true, 307);
?>
