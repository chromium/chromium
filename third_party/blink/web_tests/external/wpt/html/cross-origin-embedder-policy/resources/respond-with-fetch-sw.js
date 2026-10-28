// Responds to requests whose URL has a respond-with-fetch query parameter with a fetch of the request,
// and to those whose URL has a respond-with-fetch-of query parameter with a no-cors fetch of its value.
self.addEventListener("install", () => self.skipWaiting());

self.addEventListener("fetch", event => {
  const params = new URL(event.request.url).searchParams;
  if (params.has("respond-with-fetch"))
    event.respondWith(fetch(event.request));
  else if (params.has("respond-with-fetch-of"))
    event.respondWith(fetch(params.get("respond-with-fetch-of"), { mode: "no-cors" }));
});
