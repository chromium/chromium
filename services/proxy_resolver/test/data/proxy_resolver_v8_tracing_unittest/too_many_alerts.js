var g_iteration = 0;

function FindProxyForURL(url, host) {
  g_iteration++;

  dnsResolve(host);

  for (var i = 0; i < 50; i++) {
    alert('Gee, all these alerts are silly!');
  }

  return "PROXY foo:" + g_iteration;
}
