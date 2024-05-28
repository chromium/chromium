self.addEventListener('activate', function(event) {
  event.waitUntil(self.clients.claim());
});

let streamController;
let cache = [];

const encoder = new TextEncoder();

self.addEventListener('fetch', function(event) {
  if (event.request.url.endsWith('event-stream.php')) {
    const stream = new ReadableStream({
      start(controller) {
        streamController = controller;
        for (const item of cache) {
          streamController.enqueue(encoder.encode(item));
        }
        cache = [];
      }
    });

    event.respondWith(
      new Response(stream, {
        headers: {'Content-Type': 'text/event-stream'}
      })
    );
  }
});

async function enqueue(data) {
  try {
    streamController.enqueue(encoder.encode(data));
  } catch {
    cache.push(data);
  }
}

function close() {
  streamController.close();
  cache = [];
}
