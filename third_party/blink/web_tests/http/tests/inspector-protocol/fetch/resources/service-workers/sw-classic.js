console.log('Importing imported-classic');
importScripts('./imported-classic.js');
const IMPORTED = CLASSIC_EXPORTED_VALUE;
console.log('Finished importing imported-classic');

addEventListener('message', async event => {
  console.log('worker got message');
  event.source.postMessage([
    event.data, `imported value: ${IMPORTED}`,
    `fetch within worker: ${await fetch('/404-me').then(res => res.statusText)}`
  ]);
});
