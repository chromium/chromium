var g_iteration = 0;

function FindProxyForURL(url, host) {
  g_iteration++;

  for (var i = 0; i < g_iteration; ++i) {
    myIpAddress();
  }

  var result = '' + dnsResolve('host' + g_iteration);
  result += g_iteration;

  alert('iteration: ' + g_iteration);
  return 'PROXY ' + result + ':34';
}
