// This PAC script is invalid, because there is a missing close brace
// on the function FindProxyForURL().

function FindProxyForURL(url, host) {
  return "DIRECT";

