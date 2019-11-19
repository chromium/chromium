// PAC script which uses isInNet on both IP addresses and hosts, and calls
// isResolvable().

function FindProxyForURL(url, host) {
  var my_ip = myIpAddress();

  if (isInNet(my_ip, "172.16.0.0", "255.248.0.0")) {
    return "PROXY a:80";
  }

  if (url.substring(0, 6) != "https:" &&
      isInNet(host, "10.0.0.0", "255.0.0.0")) {
    return "PROXY b:80";
  }

  if (dnsDomainIs(host, "foo.bar.baz.com") || !isResolvable(host)) {
    return "PROXY c:100";
  }

  return "DIRECT";
}
