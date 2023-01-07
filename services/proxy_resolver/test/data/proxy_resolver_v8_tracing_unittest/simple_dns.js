var g_iteration = 0;

function FindProxyForURL(url, host) {
  g_iteration++;
  myIpAddress();
  var ip = dnsResolve(host);
  return "PROXY " + ip + '.test:' + g_iteration;
}
