console.log('Importing imported-module');
import MODULE_EXPORTED_VALUE from './imported-module.js';
const IMPORTED = MODULE_EXPORTED_VALUE;
console.log('Finished importing imported-module');

addEventListener('message', async event => {
  console.log('worker got message');
  event.source.postMessage([
    event.data, `imported value: ${IMPORTED}`,
    `fetch within worker: ${await fetch('/404-me').then(res => res.statusText)}`
  ]);
});
