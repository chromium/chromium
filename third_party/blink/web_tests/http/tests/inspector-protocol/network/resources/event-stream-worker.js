self.addEventListener('activate', function(event) {
  event.waitUntil(self.clients.claim());
});

let streamController;

const encoder = new TextEncoder();

self.addEventListener('fetch', function(event) {
  if (event.request.url.endsWith('event-stream.php')) {
    const stream = new ReadableStream({
      start(controller) {
        streamController = controller;
      }
    });

    event.respondWith(
      new Response(stream, {
        headers: {'Content-Type': 'text/event-stream'}
      })
    );
  }
});

function enqueue(data) {
  streamController.enqueue(encoder.encode(data));
}

function close() {
  streamController.close();
}
