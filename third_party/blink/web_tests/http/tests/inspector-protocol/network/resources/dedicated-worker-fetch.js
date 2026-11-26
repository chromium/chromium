self.onmessage = async event => {
  try {
    const response = await fetch(event.data, {cache: 'no-store'});
    postMessage(`fetch succeeded with status ${response.status}`);
  } catch (error) {
    postMessage('fetch failed');
  }
};
postMessage('ready');
