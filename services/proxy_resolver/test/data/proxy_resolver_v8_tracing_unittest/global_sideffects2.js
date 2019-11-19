var g_iteration = 0;

function FindProxyForURL(url, host) {
  g_iteration++;

  var ips;

  if (g_iteration < 3) {
    ips = [
      dnsResolve('host1'),
      dnsResolve('host2')
    ];
  } else {
    ips = [ dnsResolve('host' + g_iteration) ];
  }

  return 'PROXY ' + ips.join('-') + ':100';
}
