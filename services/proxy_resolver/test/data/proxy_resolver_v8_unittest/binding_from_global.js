// Calls a bindings outside of FindProxyForURL(). This causes the code to
// get exercised during initialization.

var x = myIpAddress();

function FindProxyForURL(url, host) {
  return "PROXY " + x + ":80";
}
