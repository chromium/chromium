async function preventBFCache() {
    await new Promise(resolve => {
      // Use a random UUID as the (highly likely) unique lock name.
      navigator.locks.request(Math.random(), async () => {
        resolve();
        // Wait forever.
        await new Promise(r => { });
      });
    });
  }

await preventBFCache();