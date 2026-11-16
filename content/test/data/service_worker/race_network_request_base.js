// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
const composeCustomResponse = () => {
  const headers = new Headers();
  headers.append('Content-Type', 'text/html');
  headers.append('X-Response-From', 'fetch-handler');
  const options = {
    status: 200,
    statusText: 'Custom response from fetch handler',
    headers
  };

  return new Response(
      '[ServiceWorkerRaceNetworkRequest] Response from the fetch handler',
      options);
};

const errors = [];

self.addEventListener('activate', e => {
  e.waitUntil(clients.claim());
});

self.addEventListener('fetch', e => {
  const {request} = e;
  const url = new URL(request.url);

  // Force slow response
  if (url.search.includes('sw_slow')) {
    const start = Date.now();
    while (true) {
      if (Date.now() - start > 1500) {
        break;
      }
    }
  }

  // Force fallback
  if (url.search.includes('sw_fallback')) {
    return;
  }

  // Force respond from the cache
  if (url.search.includes('sw_respond')) {
    e.respondWith(composeCustomResponse());
  }

  if (url.search.includes('sw_pass_through')) {
    e.respondWith(fetch(request).catch(err => {
      errors.push(err);
    }));
  }

  if (url.search.includes('sw_clone_pass_through')) {
    e.respondWith(fetch(request.clone()).catch(err => {
      errors.push(err);
    }));
  }

  if (url.search.includes('sw_report_fetch_error')) {
    const serializeError = err => ({
      isTypeError: err instanceof TypeError,
      name: err ? err.name : '',
      message: err ? err.message : '',
      stack: err && err.stack ? String(err.stack) : ''
    });
    e.respondWith(
        fetch(request)
            .then(() => new Response(JSON.stringify({unexpectedSuccess: true})))
            .catch(err => new Response(JSON.stringify(serializeError(err)), {
                     headers: {'Content-Type': 'application/json'}
                   })));
    return;
  }

  if (url.search.includes('sw_report_second_fetch_error')) {
    const serializeError = err => ({
      isTypeError: err instanceof TypeError,
      name: err ? err.name : '',
      message: err ? err.message : ''
    });
    e.respondWith((async () => {
      let firstError = null;
      let secondError = null;
      let cloneError = null;
      const clonePromise =
          fetch(request.clone()).catch(err => serializeError(err));
      try {
        await fetch(request);
      } catch (err) {
        firstError = serializeError(err);
      }
      cloneError = await clonePromise;
      try {
        await fetch(request);
      } catch (err) {
        secondError = serializeError(err);
      }
      return new Response(
          JSON.stringify({firstError, secondError, cloneError}),
          {headers: {'Content-Type': 'application/json'}});
    })());
    return;
  }

  if (url.search.includes('sw_report_abort_signal_error')) {
    e.respondWith((async () => {
      const controller = new AbortController();
      const promise = fetch(request, {signal: controller.signal});
      controller.abort();
      try {
        await promise;
        return new Response(JSON.stringify({unexpectedSuccess: true}));
      } catch (err) {
        return new Response(
            JSON.stringify({
              isTypeError: err instanceof TypeError,
              name: err ? err.name : '',
              message: err ? err.message : ''
            }),
            {headers: {'Content-Type': 'application/json'}});
      }
    })());
    return;
  }

  if (url.search.includes('sw_synthetic_reply_and_record_race_error')) {
    e.waitUntil((async () => {
      try {
        await fetch(request);
      } catch (err) {
        const serialized = {
          isTypeError: err instanceof TypeError,
          name: err ? err.name : '',
          message: err ? err.message : '',
          stack: err && err.stack ? String(err.stack) : ''
        };
        errors.push(serialized);
        const allClients = await clients.matchAll({includeUncontrolled: true});
        for (const client of allClients) {
          client.postMessage({type: 'race_error_recorded', error: serialized});
        }
      }
    })());
    e.respondWith(composeCustomResponse());
    return;
  }

  if (url.search.includes('sw_report_post_headers_abort')) {
    e.respondWith((async () => {
      try {
        const response = await fetch(request);
        let bodyError = null;
        try {
          await response.text();
        } catch (err) {
          bodyError = {
            isTypeError: err instanceof TypeError,
            name: err ? err.name : '',
            message: err ? err.message : ''
          };
        }
        return new Response(
            JSON.stringify(
                {fetchResolved: true, status: response.status, bodyError}),
            {headers: {'Content-Type': 'application/json'}});
      } catch (err) {
        return new Response(
            JSON.stringify(
                {fetchResolved: false, message: err ? err.message : ''}),
            {headers: {'Content-Type': 'application/json'}});
      }
    })());
    return;
  }
});

self.addEventListener('message', e => {
  if (e.data == 'errors') {
    e.source.postMessage(errors);
  }
});
