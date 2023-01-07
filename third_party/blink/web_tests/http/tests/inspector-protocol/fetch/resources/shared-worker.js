self.testToken = 'FAIL: original value';

self.addEventListener('connect', e => {
  e.ports[0].postMessage('ready');
});

