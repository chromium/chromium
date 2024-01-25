// Helper functions for ServiceWorker static routing API.
//
// test-helpers.sub.js must be loaded before using this.

// Get a dictionary of information recorded inside ServiceWorker.
// It includes:
// - request URL and mode.
// - errors.
//
// See: static-router-sw.js for details.
const get_info_from_worker = async worker => {
  const promise = new Promise(function(resolve) {
      var channel = new MessageChannel();
      channel.port1.onmessage = function(msg) { resolve(msg); };
      worker.postMessage({port: channel.port2}, [channel.port2]);
    });
  const message = await promise;

  return message.data;
}

// Reset information stored inside ServiceWorker.
const reset_info_in_worker = async worker => {
  const promise = new Promise(function(resolve) {
      var channel = new MessageChannel();
      channel.port1.onmessage = function(msg) { resolve(msg); };
      worker.postMessage({port: channel.port2, reset: true}, [channel.port2]);
    });
  await promise;
}

// Register the ServiceWorker and wait until activated.
// {ruleKey} represents the key of routerRules defined in router-rules.js.
const registerAndActivate = async (test, ruleKey) => {
  const swScript = 'resources/static-router-sw.js';
  const swURL = `${swScript}?key=${ruleKey}`;
  const swScope = 'resources/';
  const reg = await service_worker_unregister_and_register(
    test, swURL, swScope, { type: 'module' });
  add_completion_callback(() => reg.unregister());
  const worker = reg.installing;
  await wait_for_state(test, worker, 'activated');

  return worker;
};
