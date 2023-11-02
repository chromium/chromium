addEventListener('message', async event => {
  console.log('worker got message');
  const resp = await fetch('/request-within-service-worker')
  event.source.postMessage(resp.status);
});
