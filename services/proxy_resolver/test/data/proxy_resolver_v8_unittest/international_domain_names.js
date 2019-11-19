// Try resolving hostnames containing non-ASCII characters.

function FindProxyForURL(url, host) {
  // This international hostname has a non-ASCII character. It is represented
  // in punycode as 'xn--bcher-kva.ch'
  var idn = 'B\u00fccher.ch';

  // We disregard the actual return value -- all we care about is that on
  // the C++ end the bindings were passed the punycode equivalent of this
  // unicode hostname.
  dnsResolve(idn);
  dnsResolveEx(idn);

  return "DIRECT";
}

