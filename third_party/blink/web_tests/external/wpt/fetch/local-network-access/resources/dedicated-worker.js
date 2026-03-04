
async function doFetch(url) {
  const response = await fetch(url);
  const body = await response.text();
  return {
    ok: response.ok,
    error: response.error,
    body,
  };
}

async function fetchAndPost(url) {
  try {
    const message = await doFetch(url);
    self.postMessage(message);
  } catch (e) {
    self.postMessage({error: e.name});
  }
}

self.onmessage = (e) => {
  fetchAndPost(e.data);
}
