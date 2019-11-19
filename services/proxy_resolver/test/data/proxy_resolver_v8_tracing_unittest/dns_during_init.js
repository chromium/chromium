var g_ips = [
  dnsResolve('host1'),
  dnsResolve('host2')
];

alert('Watsup');
alert('Watsup2');

function FindProxyForURL(url, host) {
  // Note that host1 and host2 should not resolve using the same cache as was
  // used for g_ips!
  var ips = g_ips.concat([dnsResolve('host1'), dnsResolve('host2')]);
  return 'PROXY ' + ips.join('-') + ':99';
}
