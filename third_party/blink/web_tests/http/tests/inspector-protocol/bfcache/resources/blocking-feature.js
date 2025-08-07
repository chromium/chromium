async function preventBFCache() {
    await new Promise(resolve => {
      // Use a random UUID as the (highly likely) unique lock name.
      navigator.locks.request(Math.random(), async () => {
        // Signal to the test that the lock is held, so it can proceed with
        // navigation.
        console.log('WebLockHeld');
        resolve();
        // Wait forever.
        await new Promise(r => { });
      });
    });
  }

await preventBFCache();