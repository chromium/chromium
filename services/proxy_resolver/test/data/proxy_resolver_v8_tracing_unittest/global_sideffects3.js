var g_iteration = 0;

function FindProxyForURL(url, host) {
  g_iteration++;

  var results = [];
  for (var i = 1; i <= g_iteration; ++i) {
    results.push('' + dnsResolve('host' + i));
  }

  alert('iteration: ' + g_iteration);
  return 'PROXY ' + results.join('-') + ':' + g_iteration;
}
