this.addEventListener('fetch', fetchEvent => {
  fetchEvent.respondWith(fetch(fetchEvent.request));
});
