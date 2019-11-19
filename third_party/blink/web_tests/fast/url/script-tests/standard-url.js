description("Canonicalization of standard URLs");

// Need to change the base URL to get the results to be consistent
// across platforms. We can't do it in the HTML because we need the
// original value in order to load this script.
let base = document.createElement('base');
base.href = 'file:///';
document.head.appendChild(base);

cases = [
  ["http://www.google.com/foo?bar=baz#", "http://www.google.com/foo?bar=baz#"],
  ["http://www.google.com/foo?bar=baz# \u00bb", "http://www.google.com/foo?bar=baz#%20%C2%BB"],
  ["http://[www.google.com]/", "http://[www.google.com]/"],
  ["http://www.google.com", "http://www.google.com/"],
  ["ht\ttp:@www.google.com:80/;p?#", "http://www.google.com/;p?#"],
  ["http:////////user:@google.com:99?foo", "http://user@google.com:99/?foo"],
  ["www.google.com", "file:///www.google.com"],
  ["http://192.0x00A80001", "http://192.168.0.1/"],
  // This test matches Blink's current behaviour, but the URL standard
  // does not unescape %2E.
  ["http://www/foo%2Ehtml", "http://www/foo.html"],
  ["http://user:pass@/", "http://user:pass@/"],
  ["http://%25DOMAIN:foobar@foodomain.com/", "http://%25DOMAIN:foobar@foodomain.com/"],
  // Backslashes should get converted to forward slashes.
  ["http:\\\\\\\\www.google.com\\\\foo", "http://www.google.com/foo"],
  // Busted refs shouldn't make the whole thing fail.
  ["http://www.google.com/asdf#\\ud800", "http://www.google.com/asdf#%EF%BF%BD"],
  // Basic port tests.
  ["http://foo:80/", "http://foo/"],
  ["http://foo:81/", "http://foo:81/"],
  ["httpa://foo:80/", "httpa://foo:80/"],
  ["http://foo:-80/", "http://foo:-80/"],
  ["https://foo:443/", "https://foo/"],
  ["https://foo:80/", "https://foo:80/"],
  ["ftp://foo:21/", "ftp://foo/"],
  ["ftp://foo:80/", "ftp://foo:80/"],
  ["gopher://foo:70/", "gopher://foo/"],
  ["gopher://foo:443/", "gopher://foo:443/"],
  ["ws://foo:80/", "ws://foo/"],
  ["ws://foo:81/", "ws://foo:81/"],
  ["ws://foo:443/", "ws://foo:443/"],
  ["ws://foo:815/", "ws://foo:815/"],
  ["wss://foo:80/", "wss://foo:80/"],
  ["wss://foo:81/", "wss://foo:81/"],
  ["wss://foo:443/", "wss://foo/"],
  ["wss://foo:815/", "wss://foo:815/"],
  ["http:/example.com/", "http://example.com/"],
  ["ftp:/example.com/", "ftp://example.com/"],
  ["https:/example.com/", "https://example.com/"],
  ["madeupscheme:/example.com/", "madeupscheme:/example.com/"],
  ["file:/example.com/", "file:///example.com/"],
  ["ftps:/example.com/", "ftps:/example.com/"],
  ["gopher:/example.com/", "gopher://example.com/"],
  ["ws:/example.com/", "ws://example.com/"],
  ["wss:/example.com/", "wss://example.com/"],
  ["data:/example.com/", "data:/example.com/"],
  ["javascript:/example.com/", "javascript:/example.com/"],
  ["mailto:/example.com/", "mailto:/example.com/"],
  ["http:example.com/", "http://example.com/"],
  ["ftp:example.com/", "ftp://example.com/"],
  ["https:example.com/", "https://example.com/"],
  ["madeupscheme:example.com/", "madeupscheme:example.com/"],
  ["ftps:example.com/", "ftps:example.com/"],
  ["gopher:example.com/", "gopher://example.com/"],
  ["ws:example.com/", "ws://example.com/"],
  ["wss:example.com/", "wss://example.com/"],
  ["data:example.com/", "data:example.com/"],
  ["javascript:example.com/", "javascript:example.com/"],
  ["mailto:example.com/", "mailto:example.com/"],
  // Escaping of non hierarchical URLs
  ["javascript:alert(\\t 1 \\n\\r)", "javascript:alert( 1 )"],
  ['javascript:alert(" \1 \u03B2 ")', 'javascript:alert(" %01 %CE%B2 ")'],
];

for (var i = 0; i < cases.length; ++i) {
  test_vector = cases[i][0];
  expected_result = cases[i][1];
  shouldBe("canonicalize('" + test_vector + "')",
           "'" + expected_result + "'");
}
