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
