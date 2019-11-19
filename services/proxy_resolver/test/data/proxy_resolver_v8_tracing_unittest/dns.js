var g_iteration = 0;

function FindProxyForURL(url, host) {
  alert('iteration: ' + g_iteration++);

  var ips = [
    myIpAddress(),
    dnsResolve(''),
    dnsResolveEx('host1'),
    dnsResolve('host2'),
    dnsResolve('host3'),
    myIpAddress(),
    dnsResolve('host3'),
    dnsResolveEx('host1'),
    myIpAddress(),
    dnsResolve('host2'),
    dnsResolveEx('host6'),
    myIpAddressEx(),
    dnsResolve('host1'),
  ];

  for (var i = 0; i < ips.length; ++i) {
    // Stringize everything.
    ips[i] = '' + ips[i];
  }

  var proxyHost = ips.join('-');
  proxyHost = proxyHost.replace(/[^0-9a-zA-Z.-]/g, '_');

  return "PROXY " + proxyHost + ":99";
}
