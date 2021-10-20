console.log("Worker");

self.addEventListener('message', e => {
  self.postMessage('ready');
});
