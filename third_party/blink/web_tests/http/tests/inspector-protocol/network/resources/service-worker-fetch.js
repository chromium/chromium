self.addEventListener('message', async event => {
  try {
    const response = await fetch(event.data, {cache: 'no-store'});
    event.source.postMessage(`fetch succeeded with status ${response.status}`);
  } catch (error) {
    event.source.postMessage('fetch failed');
  }
});
