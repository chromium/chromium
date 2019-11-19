function FindProxyForURL(url, host) {
  if (host == 'throw-an-error') {
    alert('Prepare to DIE!');
    var x = null;
    return x.split('-');  // Throws exception.
  }
  return "PROXY i-approve-this-message:42";
}
