async function doFetch(url) {
  const response = await fetch(url);
  const body = await response.text();
  return {
    ok: response.ok,
    error: response.error,
    body,
  };
}

async function fetchAndPost(url, port) {
  try {
    const message = await doFetch(url);
    port.postMessage(message);
  } catch (e) {
    port.postMessage({error: e.toString()});
  }
}

let webtransport;

async function doWebTransport(url, port) {
  try {
    webtransport = new WebTransport(url);
    await webtransport.ready;
    port.postMessage('open');
  } catch (error) {
    port.postMessage('error');
  }
}

let websocket;

async function doWebSocket(url, port) {
  websocket = new WebSocket(url);
  websocket.onopen = () => {
    port.postMessage('open');
  };

  websocket.onclose = (evt) => {
    port.postMessage(`close: code ${evt.code}`);
  };
}

async function handleMessage(e) {
  const port = e.ports[0];
  switch (e.data.method) {
    case 'fetch':
      fetchAndPost(e.data.url, port);
      break;
    case 'websocket':
      doWebSocket(e.data.url, port);
      break;
    case 'webtransport':
      doWebTransport(e.data.url, port);
      break;
  }
};

self.addEventListener('activate', function(event) {
  event.waitUntil(clients.claim());
});

self.addEventListener('message', e => {
  e.waitUntil(handleMessage(e));
});
