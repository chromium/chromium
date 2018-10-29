self.addEventListener('push', async function (event) {
  const clients = await self.clients.matchAll({includeUncontrolled: true});
  for (const c of clients)
    c.postMessage(event.data.text());
});
