<?php
header('Cross-Origin-Embedder-Policy: require-corp');
header('Content-type: text/javascript');
?>
self.addEventListener('message', (m) => {
  const sharedArray = new Int32Array(m.data);
  var i = 0;
  // Notify main thread that execution started.
  Atomics.store(sharedArray, i++, 1);
  // Wait for the main thread to enter the blocking loop.
  Atomics.wait(sharedArray, i++, 1);
  // Make the update for the main thread to exit the loop.
  Atomics.store(sharedArray, i, 1);
});