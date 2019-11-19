// Return a single-proxy result, which encodes ALL the arguments that were
// passed to FindProxyForURL().

function FindProxyForURL(url, host) {
  if (arguments.length != 2) {
    throw "Wrong number of arguments passed to FindProxyForURL!";
    return "FAIL";
  }

  return "PROXY " + makePseudoHost(url + "." + host);
}

// Form a string that kind-of resembles a host. We will replace any
// non-alphanumeric character with a dot, then fix up the oddly placed dots.
function makePseudoHost(str) {
  var result = "";

  for (var i = 0; i < str.length; ++i) {
    var c = str.charAt(i);
    if (!isValidPseudoHostChar(c)) {
      c = '.';  // Replace unsupported characters with a dot.
    }

    // Take care not to place multiple adjacent dots,
    // a dot at the beginning, or a dot at the end.
    if (c == '.' &&
        (result.length == 0 || 
         i == str.length - 1 ||
         result.charAt(result.length - 1) == '.')) {
      continue;
    }
    result += c;
  }
  return result;
}

function isValidPseudoHostChar(c) {
  if (c >= '0' && c <= '9')
    return true;
  if (c >= 'a' && c <= 'z')
    return true;
  if (c >= 'A' && c <= 'Z')
    return true;
  return false;
}
