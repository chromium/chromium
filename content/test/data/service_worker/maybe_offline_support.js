// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function sleep(delay) {
  return new Promise(resolve => {
    setTimeout(() => {
      resolve();
    }, delay);
  });
}

const scriptUrlParams = new URL(self.serviceWorker.scriptURL).searchParams;
if (scriptUrlParams.has('pendingInstallEvent')) {
  // Prevents this SW from being activated.
  self.addEventListener('install', e => {
    e.waitUntil(new Promise(resolve => {}));
  });
}

self.addEventListener("fetch", event => {
  const param = new URL(event.request.url).searchParams;

  if (param.has("fetch")) {
    event.respondWith(fetch(event.request.url));
  } else if (param.has("offline")) {
    event.respondWith(new Response("Hello Offline page"));
  } else if (param.has("fetch_or_offline")) {
    event.respondWith(
      fetch(event.request).catch(error => {
        return new Response("Hello Offline page");
      })
    );
  } else if (param.has("sleep_then_fetch")) {
    event.respondWith(
      sleep(param.get("sleep") || 0).then(() => {
        return fetch(event.request.url);
      })
    );
  } else if (param.has("sleep_then_offline")) {
    event.respondWith(
      sleep(param.get("sleep") || 0).then(() => {
        return new Response("Hello Offline page");
      })
    );
  } else if (param.has("cache_add")) {
    event.respondWith((async () => {
      const cache = await caches.open('maybe_offline_support_cache_add');
      await cache.add(event.request);
      return cache.match(event.request);
    })());
  } else if (param.has("redirect")) {
    const headers = new Headers();
    headers.append("Location", "https://a.com");
    event.respondWith(
      new Response("Redirect", {"status": 301, "headers": headers})
    );
  } else {
    // fallback case: do nothing.
  }
});
