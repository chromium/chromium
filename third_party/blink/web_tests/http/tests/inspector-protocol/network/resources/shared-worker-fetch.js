self.addEventListener('connect', e => {
  const port = e.ports[0];
  port.onmessage = async event => {
    try {
      const response = await fetch(event.data, {cache: 'no-store'});
      port.postMessage(`fetch succeeded with status ${response.status}`);
    } catch (error) {
      port.postMessage('fetch failed');
    }
  };
});
