<?php

if (isset($_GET['corp'])) {
  header("Cross-Origin-Resource-Policy: " . $_GET['corp']);
}

if (isset($_GET['coop'])) {
  $coop = $_GET['coop'] ? $_GET['coop'] : "same-origin";
  header("Cross-Origin-Opener-Policy: $coop");
}

if (isset($_GET['coep'])) {
  $coep = $_GET['coep'] ? $_GET['coep'] : "require-corp";
  header("Cross-Origin-Embedder-Policy: $coep");
}

if (isset($_GET['coop-rpt'])) {
  $coop = $_GET['coop-rpt'] ? $_GET['coop-rpt'] : "same-origin";
  header("Cross-Origin-Opener-Policy-Report-Only: $coop");
}

if (isset($_GET['coep-rpt'])) {
  $coep = $_GET['coep-rpt'] ? $_GET['coep-rpt'] : "require-corp";
  header("Cross-Origin-Embedder-Policy-Report-Only: $coep");
}

if (isset($_GET['ctype'])) {
  $ctype = $_GET['ctype'] ? $_GET['ctype'] : "text/html";
  header("Content-Type: $ctype");
}

$content = isset($_GET['content']) ? $_GET['content'] : "This is some text";
echo $content;

?>
