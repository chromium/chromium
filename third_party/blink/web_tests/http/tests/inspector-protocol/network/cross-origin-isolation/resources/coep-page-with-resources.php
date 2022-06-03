<?php

header("Cross-Origin-Embedder-Policy: require-corp");

?>

<!DOCTYPE html>
<html lang="en">
  <head>
    <title>Page with Cross-Origin-Embedder-Policy</title>
    <meta charset="utf-8">
  </head>
  <body>
    <div>
      This is a COEP page embedding resources.
    </div>
    None/None<br/>
    <iframe src="https://devtools.oopif.test:8443/inspector-protocol/network/cross-origin-isolation/resources/page-with-coep-corp.php">
    </iframe><br/>
    COEP/None<br/>
    <iframe src="https://devtools.oopif.test:8443/inspector-protocol/network/cross-origin-isolation/resources/page-with-coep-corp.php?coep">
    </iframe><br/>
    COEP/Same-Site<br/>
    <iframe src="https://devtools.oopif.test:8443/inspector-protocol/network/cross-origin-isolation/resources/page-with-coep-corp.php?coep&corp=same-site">
    </iframe><br/>
    COEP/Same-Origin<br/>
    <iframe src="https://devtools.oopif.test:8443/inspector-protocol/network/cross-origin-isolation/resources/page-with-coep-corp.php?coep&corp=same-origin">
    </iframe><br/>
    COEP/Cross-Origin<br/>
    <iframe src="https://devtools.oopif.test:8443/inspector-protocol/network/cross-origin-isolation/resources/page-with-coep-corp.php?coep&corp=cross-origin">
    </iframe><br/>
    Script CORP None <span id="script-corp-none">not loaded</span>
    <script src="https://devtools.oopif.test:8443/inspector-protocol/network/cross-origin-isolation/resources/page-with-coep-corp.php?script&" defer>
    </script><br/>
    Script CORP cross origin <span id="script-corp-cross-origin">not loaded</span>
    <script src="https://devtools.oopif.test:8443/inspector-protocol/network/cross-origin-isolation/resources/page-with-coep-corp.php?script&corp=cross-origin" defer>
    </script>    <br/>
    Script CORP same site <span id="script-corp-same-site">not loaded</span>
    <script src="https://devtools.oopif.test:8443/inspector-protocol/network/cross-origin-isolation/resources/page-with-coep-corp.php?script&corp=same-site" defer>
    </script><br/>
    Script CORP same origin <span id="script-corp-same-origin">not loaded</span>
    <script src="https://devtools.oopif.test:8443/inspector-protocol/network/cross-origin-isolation/resources/page-with-coep-corp.php?script&corp=same-origin" defer>
    </script><br/>
    Sandboxed COOP iframe<br/>
    <iframe src="https://devtools.oopif.test:8443/inspector-protocol/network/cross-origin-isolation/resources/page-with-coep-corp.php?coop">
    </iframe><br/>
  </body>
</html>

