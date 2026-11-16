<?php
// Responses for the socket-layer throttling tests. Each response closes its
// connection, so that every request pays for a new connection.
header('Connection: close');
header('Cache-Control: no-store');
switch ($_GET['type'] ?? '') {
  case 'html':
    header('Content-Type: text/html');
    echo '<!DOCTYPE html><title>Socket-layer throttling</title>';
    break;
  case 'script':
    header('Content-Type: text/javascript');
    echo '/* ok */';
    break;
  case 'incompressible':
    // ~100KB of base64-encoded random bytes.
    header('Content-Type: text/plain');
    echo base64_encode(random_bytes(75 * 1024));
    break;
  case 'compressible':
    // 100KB of a single repeated character, ~500 bytes on the wire.
    ob_start('ob_gzhandler');
    header('Content-Type: text/plain');
    echo str_repeat('a', 100 * 1024);
    break;
  case 'cacheable':
    // Fresh for an hour, so that later requests for it hit the HTTP cache.
    header('Cache-Control: max-age=3600');
    header('Content-Type: text/plain');
    echo 'ok';
    break;
  default:
    header('Content-Type: text/plain');
    echo 'ok';
}
