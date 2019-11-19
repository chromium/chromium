// U+200B is the codepoint for zero-width-space.
function FindProxyForURL(url, host) {
  return "PROXY foo.com\u200B";
}
