<?php
header('Content-Type: application/javascript');
header('Cross-Origin-Embedder-Policy: require-corp');
?>
// Fetches a URL in a loop and reports the outcomes on a BroadcastChannel, so
// that a page other than the worker's owner can observe them.
self.onmessage = async event => {
  const {sab, channelName} = event.data;
  const releaseFlag = sab ? new Int32Array(sab) : null;
  const channel = new BroadcastChannel(channelName);
  channel.onmessage = msg => {
    if (msg.data === 'release' && releaseFlag) {
      Atomics.store(releaseFlag, 0, 1);
    }
  };

  while (true) {
    try {
      await fetch('hello-world.html', {cache: 'no-store'});
      channel.postMessage('succeeded');
    } catch (error) {
      channel.postMessage('failed');
    }
    // A request that fails offline fails immediately. Don't flood the channel.
    await new Promise(resolve => setTimeout(resolve, 5));
  }
};
