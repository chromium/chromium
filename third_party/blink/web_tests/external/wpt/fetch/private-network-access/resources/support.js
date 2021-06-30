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
// `options.protocol` and `options.port`, if defined, override the respective
// properties of the returned URL object.
function resolveUrl(url, options) {
  const result = new URL(url, window.location);
  if (options === undefined) {
    return result;
  }

  const { port, protocol } = options;
  if (port !== undefined) {
    result.port = port;
  }
  if (protocol !== undefined) {
    result.protocol = protocol;
  }

  return result;
}

const kDefaultSourcePath = "resources/fetcher.html";

const kTreatAsPublicAddressSuffix =
      "?pipe=header(Content-Security-Policy,treat-as-public-address)";

function sourceUrl({ protocol, port, treatAsPublicAddress }) {
  let path = kDefaultSourcePath;
  if (treatAsPublicAddress) {
    path += kTreatAsPublicAddressSuffix;
  }

  return resolveUrl(path, { protocol, port });
}

const kFetchTestResult = {
  success: true,
  failure: "TypeError: Failed to fetch",
}

// Runs a fetch test. Tries to fetch a given subresource from a given document.
//
// Main argument shape:
//
//   {
//     // Optional.
//     source: {  // Optional, all fields optional.
//       // Optional. The protocol of the URL of the initiator document.
//       protocol,
//
//       // Optional. The port of the URL of the initiator document.
//       port,
//
//       // Optional. If true, the initiator document sets the
//       // `treat-as-public-address` CSP directive.
//       treatAsPublicAddress,
//     },
//
//     // Optional.
//     target: {
//       // The protocol of the URL of the target subresource.
//       protocol,
//
//       // The port of the URL of the target subresource.
//       port,
//     },
//
//     // Required. The expected result of the fetch. Can be:
//     // - `true` for a successful fetch
//     // - `false` for a successful opaque fetch
//     // - a string representation of an exception for a failed fetch
//     expected,
//   }
//
async function fetchTest(t, { source, target, expected }) {
  if (source === undefined) {
    source = {};
  }
  const iframe = await appendIframe(t, document, sourceUrl(source));

  const targetUrl = resolveUrl("/common/blank-with-cors.html", target);
  const reply = futureMessage();
  iframe.contentWindow.postMessage(targetUrl.href, "*");
  assert_equals(await reply, expected);
}
