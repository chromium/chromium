async function preventBFCache() {
  await new Promise(resolve => {
    navigator.keyboard.lock();
    resolve();
  });
  }

await preventBFCache();