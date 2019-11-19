self.installPromise = new Promise(fulfill => self.installCallback = fulfill);

self.addEventListener('install', event => {
  if (location.search === '?defer-install')
    event.waitUntil(self.installPromise);
});

self.addEventListener('fetch', fetchEvent => {
  if (/fulfill-by-sw/.test(fetchEvent.request.url)) {
    fetchEvent.respondWith(new Response("response from service worker"));
    return;
  }

  fetchEvent.respondWith(fetch(fetchEvent.request));
});
