var g_iteration = 0;

function FindProxyForURL(url, host) {
  g_iteration++;

  var ips = [
    dnsResolve('host1'),
    dnsResolve('crazy' + g_iteration)
  ];

  alert('iteration: ' + g_iteration);

  return 'PROXY ' + ips.join('-') + ':100';
}
