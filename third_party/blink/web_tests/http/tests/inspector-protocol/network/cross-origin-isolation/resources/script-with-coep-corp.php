<?php

if (isset($_GET['corp'])) {
  header("Cross-Origin-Resource-Policy: " . $_GET['corp']);
}

if (isset($_GET['coop'])) {
  header("Cross-Origin-Opener-Policy: same-origin");
}


if (isset($_GET['coep'])) {
  header("Cross-Origin-Embedder-Policy: require-corp");
}

console.log("Hello from script");

?>
