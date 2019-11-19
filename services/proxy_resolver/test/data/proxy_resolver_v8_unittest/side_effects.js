if (!gCounter) {
  // We write it this way so if the script gets loaded twice,
  // gCounter remains dirty.
  var gCounter = 0;
}

function FindProxyForURL(url, host) {
  return "PROXY sideffect_" + gCounter++;
}

