const cacheName = 'wasm-cache-test';
const assets = [
  '/wasm/resources/load-wasm.php?name=large.wasm&cors',
  '/wasm/resources/load-wasm.php?name=small.wasm&cors',
];

self.addEventListener('install', (event) => {
    event.waitUntil(
      caches.open(cacheName)
        .then((cache) => cache.addAll(assets)));
  });

self.addEventListener('fetch', (event) => {
    event.respondWith(async function() {
      const cache = await caches.open(cacheName);
      const response = await cache.match(event.request);
      console.log(`${event.request.url} found response ${response}`);
      if (response)
        return response;
      return fetch(event.request);
    }());
  });
