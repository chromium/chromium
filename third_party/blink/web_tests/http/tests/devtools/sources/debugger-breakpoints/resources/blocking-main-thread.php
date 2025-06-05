<?php
header('Cross-Origin-Opener-Policy: same-origin');
header('Cross-Origin-Embedder-Policy: require-corp');
header('Content-Type: text/html');
?>
<html>
<script>
  const sab = new SharedArrayBuffer(1024);
  const sharedArray = new Int32Array(sab.buffer);
  const worker = new Worker('blocking-main-thread-worker.php');
  worker.postMessage(sab);

  function checkWorkerReady() {
    // Wait for the worker to start execution.
    if (sharedArray[0] === 1) {
      // Enter blocking loop, and wait for an update before exiting.
      while (sharedArray[2] != 1) Atomics.notify(sharedArray, 1, 1);
    } else {
      setTimeout(checkWorkerReady, 0);
    }
  }
  // Let worker start execution.
  setTimeout(checkWorkerReady, 0);
</script>
</html>