self.addEventListener('sync', event => {
  // Keeps this event alive.
  event.waitUntil(new Promise(function () { }));
});

self.addEventListener('periodicsync', event => {
  // Keeps this event alive.
  event.waitUntil(new Promise(function () { }));
});

// We need this fetch handler to check the sanity of the SW using iframe().
self.addEventListener('fetch', event => {
  event.respondWith(new Response(''));
});
