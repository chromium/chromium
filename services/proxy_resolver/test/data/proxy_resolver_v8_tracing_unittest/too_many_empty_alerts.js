var g_iteration = 0;

function FindProxyForURL(url, host) {
  g_iteration++;

  dnsResolve(host);

  for (var i = 0; i < 1000; i++) {
    alert('');
  }

  return "PROXY foo:" + g_iteration;
}
