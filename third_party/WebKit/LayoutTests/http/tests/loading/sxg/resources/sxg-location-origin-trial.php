<?php
header('HTTP/1.0 200 OK');
// generate_token.py http://127.0.0.1:8000 SignedHTTPExchangeOriginTrial --expire-timestamp=2000000000
header("Origin-Trial: AqVs+066F+i9lgx2N0bCs5B31heTh9PJOfVM00yDS34V3FqBzWCuI3VKOpHPDeeujnaqJRVPHG8q+jBXjPNoLwkAAABleyJvcmlnaW4iOiAiaHR0cDovLzEyNy4wLjAuMTo4MDAwIiwgImZlYXR1cmUiOiAiU2lnbmVkSFRUUEV4Y2hhbmdlT3JpZ2luVHJpYWwiLCAiZXhwaXJ5IjogMjAwMDAwMDAwMH0=");
header("Content-Type: application/signed-exchange;v=b2");
$name = 'sxg-location.sxg';
$fp = fopen($name, 'rb');
header("Content-Length: " . filesize($name));
fpassthru($fp);
?>
