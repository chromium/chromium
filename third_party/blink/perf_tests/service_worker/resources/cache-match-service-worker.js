importScripts('/resources/worker-test-helper.js');

function sendTestResultToClients(result) {
  clients.matchAll({includeUncontrolled: true}).then((allClients) => {
    for (const client of allClients) {
      client.postMessage(result);
    }
  });
}

const FILES = [];
for (let i = 0; i < 1000; i++) {
  FILES.push(`/service_worker/resources/data/1K_${i}.txt`);
}

const IGNORE_SEARCH =
    new URLSearchParams(self.location.search).has('ignoreSearch');
const CACHE_NAME = IGNORE_SEARCH ? 'cache-match-ignore-search' : 'cache-match';
let cache = null;
async function setup() {
  await caches.delete(CACHE_NAME);
  cache = await caches.open(CACHE_NAME);
  await cache.addAll(FILES);
}

let iteration = 0;
async function cacheMatch() {
  const request = FILES[(iteration * 7) % 1000];
  const response = IGNORE_SEARCH ?
      await cache.match(`${request}?query=ignored`, {ignoreSearch: true}) :
      await cache.match(request);
  await response.arrayBuffer();
  ++iteration;
}

async function test() {
  const result = await self.workerTestHelper.measureTime(
      {setup: setup, run: cacheMatch, iterationCount: 100});
  sendTestResultToClients(result);
}

self.addEventListener('install', function(event) {
  event.waitUntil(test());
});
