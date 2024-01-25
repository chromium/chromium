'use strict';

import {routerRules} from './router-rules.js';

var requests = [];
var errors = [];

self.addEventListener('install', async e => {
  e.waitUntil(caches.open('v1').then(
      cache => {cache.put('cache.txt', new Response('From cache'))}));

  const params = new URLSearchParams(location.search);
  const key = params.get('key');
  try {
    await e.addRoutes(routerRules[key]);
  } catch (e) {
    errors.push(e);
  }
  self.skipWaiting();
});

self.addEventListener('activate', e => {
  e.waitUntil(clients.claim());
});

self.addEventListener('fetch', function(event) {
  requests.push({url: event.request.url, mode: event.request.mode});
  const url = new URL(event.request.url);
  const nonce = url.searchParams.get('nonce');
  event.respondWith(new Response(nonce));
});

self.addEventListener('message', function(event) {
  let r = requests;
  let e = errors;
  if (event.data.reset) {
    requests = [];
    errors = [];
  }
  if (event.data.port) {
    event.data.port.postMessage({requests: r, errors: e});
  }
});
