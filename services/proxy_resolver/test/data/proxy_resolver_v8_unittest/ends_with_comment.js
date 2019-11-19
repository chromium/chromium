function FindProxyForURL(url, host) {
  return "PROXY success:80";
}

// We end the script with a comment (and no trailing newline).
// This used to cause problems, because internally ProxyResolverV8
// would append some functions to the script; the first line of
// those extra functions was being considered part of the comment.