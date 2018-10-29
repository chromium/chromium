self.addEventListener('activate', event => {
    event.waitUntil(
      registration.navigationPreload.enable()
        .then(_ => registration.navigationPreload.setHeaderValue('hello')));
  });

self.addEventListener('fetch', event => {
    if (event.request.url.indexOf('BrokenChunked') != -1) {
      event.respondWith(
        event.preloadResponse.then(r => r.text())
          .catch(_ => { return new Response('dummy'); }));
      return;
    }
    if (event.preloadResponse) {
      event.respondWith(event.preloadResponse);
    }
  });
