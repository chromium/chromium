// Creates a new iframe in |doc|, calls |func| on it and appends it as a child
// of |doc|.
// Returns a promise that resolves to the iframe once loaded (successfully or
// not).
// The iframe is removed from |doc| once test |t| is done running.
//
// NOTE: Because iframe elements always invoke the onload event handler, even
// in case of error, we cannot wire onerror to a promise rejection. The Promise
// constructor requires users to resolve XOR reject the promise.
function appendIframeWith(t, doc, func) {
  return new Promise(resolve => {
      const child = doc.createElement("iframe");
      func(child);
      child.onload = () => { resolve(child); };
      doc.body.appendChild(child);
      t.add_cleanup(() => { doc.body.removeChild(child); });
    });
}

// Appends a child iframe to |doc| sourced from |src|.
//
// See append_child_frame_with() for more details.
function appendIframe(t, doc, src) {
  return appendIframeWith(t, doc, child => { child.src = src; });
}

// Register an event listener that will resolve this promise when this
// window receives a message posted to it.
function futureMessage() {
  return new Promise(resolve => {
      window.addEventListener("message", e => resolve(e.data));
  });
};

// Resolves a URL relative to the current location, returning an absolute URL.
//
// `url` specifies the relative URL, e.g. "foo.html" or "http://foo.example".
// `options`, if defined, should have the following shape:
//
//   {
//     // Optional. Overrides the protocol of the returned URL.
//     protocol,
//
//     // Optional. Overrides the port of the returned URL.
//     port,
//
//     // Extra headers.
//     headers,
//
//     // Extra search params.
//     searchParams,
//   }
//
function resolveUrl(url, options) {
  const result = new URL(url, window.location);
  if (options === undefined) {
    return result;
  }

  const { port, protocol, headers, searchParams } = options;
  if (port !== undefined) {
    result.port = port;
  }
  if (protocol !== undefined) {
    result.protocol = protocol;
  }
  if (headers !== undefined) {
    const pipes = [];
    for (key in headers) {
      pipes.push(`header(${key},${headers[key]})`);
    }
    result.searchParams.append("pipe", pipes.join("|"));
  }
  if (searchParams !== undefined) {
    for (key in searchParams) {
      result.searchParams.append(key, searchParams[key]);
    }
  }

  return result;
}

const kFetchTestResult = {
  success: {
    ok: true,
    body: "success",
  },
  opaque: {
    ok: false,
    type: "opaque",
    body: "",
  },
  failure: {
    error: "TypeError: Failed to fetch",
  },
}

// Runs a fetch test. Tries to fetch a given subresource from a given document.
//
// Main argument shape:
//
//   {
//     // Optional. Options for `resolveUrl()` when computing the source URL.
//     source,
//
//     // Optional. Options for `resolveUrl()` when computing the target URL.
//     // See the documentation of `resources/preflight.py` for details on
//     // valid `searchParams` and their effect.
//     target,
//
//     // Fetch options.
//     fetchOptions,
//
//     // Required. One of the values in `kFetchTestResult`.
//     expected,
//   }
//
async function fetchTest(t, { source, target, fetchOptions, expected }) {
  const sourceUrl = resolveUrl("resources/fetcher.html", source);
  const iframe = await appendIframe(t, document, sourceUrl);

  const targetUrl = resolveUrl("resources/preflight.py", target);
  const message = {
    url: targetUrl.href,
    options: fetchOptions,
  };

  const reply = futureMessage();
  iframe.contentWindow.postMessage(message, "*");

  const { error, ok, type, body } = await reply;

  assert_equals(error, expected.error, "error");

  assert_equals(ok, expected.ok, "response ok");
  assert_equals(body, expected.body, "response body");

  if (expected.type !== undefined) {
    assert_equals(type, expected.type, "response type");
  }
}

const kWebsocketTestResult = {
  success: "open",

  // The code is a best guess. It is not yet entirely specified, so it may need
  // to be changed in the future based on implementation experience.
  failure: "close: code 1006",
};

// Runs a websocket test. Attempts to open a websocket from `source` (in an
// iframe) to `target`, then checks that the result is as `expected`.
//
// Argument shape is same as for `fetchTest`, except for the following:
//
//   `expected` should be one of the values in `kWebsocketTestResult`.
//
async function websocketTest(t, { source, target, expected }) {
  const sourceUrl = resolveUrl("resources/socket-opener.html", source);
  const iframe = await appendIframe(t, document, sourceUrl);

  const targetUrl = resolveUrl("/echo", target);
  const reply = futureMessage();
  iframe.contentWindow.postMessage(targetUrl.href, "*");
  assert_equals(await reply, expected);
}
