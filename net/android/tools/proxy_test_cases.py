#!/usr/bin/env python
# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generator script for proxy tests.

See AndroidProxySelectorTest.java
and net/proxy_resolution/proxy_config_service_android_unittest.cc

To generate C++, run this script without arguments.
To generate Java, run this script with -j argument.

Note that this generator is not run as part of the build process because
we are assuming that these test cases will not change often.
"""

import optparse

test_cases = [
  {
    "name": "NoProxy",
    "description" : "Test direct mapping when no proxy defined.",
    "properties" : {
    },
    "mappings" : {
      "http://example.com/" : "DIRECT",
      "ftp://example.com/" : "DIRECT",
      "https://example.com/" : "DIRECT",
    }
  },
  {
    "name": "HttpProxyHostAndPort",
    "description" : "Test http.proxyHost and http.proxyPort works.",
    "properties" : {
      "http.proxyHost" : "httpproxy.com",
      "http.proxyPort" : "8080",
    },
    "mappings" : {
      "http://example.com/" : "PROXY httpproxy.com:8080",
      "ftp://example.com/" : "DIRECT",
      "https://example.com/" : "DIRECT",
    }
  },
  {
    "name": "HttpProxyHostOnly",
    "description" : "We should get the default port (80) for proxied hosts.",
    "properties" : {
      "http.proxyHost" : "httpproxy.com",
    },
    "mappings" : {
      "http://example.com/" : "PROXY httpproxy.com:80",
      "ftp://example.com/" : "DIRECT",
      "https://example.com/" : "DIRECT",
    }
  },
  {
    "name": "HttpProxyPortOnly",
    "description" :
        "http.proxyPort only should not result in any hosts being proxied.",
    "properties" : {
      "http.proxyPort" : "8080",
    },
    "mappings" : {
      "http://example.com/" : "DIRECT",
      "ftp://example.com/" : "DIRECT",
      "https://example.com/" : "DIRECT"
    }
  },
  {
    "name": "HttpNonProxyHosts1",
    "description" : "Test that HTTP non proxy hosts are mapped correctly",
    "properties" : {
      "http.nonProxyHosts" : "slashdot.org",
      "http.proxyHost" : "httpproxy.com",
      "http.proxyPort" : "8080",
    },
    "mappings" : {
      "http://example.com/" : "PROXY httpproxy.com:8080",
      "http://slashdot.org/" : "DIRECT",
    }
  },
  {
    "name": "HttpNonProxyHosts2",
    "description" : "Test that | pattern works.",
    "properties" : {
      "http.nonProxyHosts" : "slashdot.org|freecode.net",
      "http.proxyHost" : "httpproxy.com",
      "http.proxyPort" : "8080",
    },
    "mappings" : {
      "http://example.com/" : "PROXY httpproxy.com:8080",
      "http://slashdot.org/" : "DIRECT",
      "http://freecode.net/" : "DIRECT",
    }
  },
  {
    "name": "HttpNonProxyHosts3",
    "description" : "Test that * pattern works.",
    "properties" : {
      "http.nonProxyHosts" : "*example.com",
      "http.proxyHost" : "httpproxy.com",
      "http.proxyPort" : "8080",
    },
    "mappings" : {
      "http://example.com/" : "DIRECT",
      "http://www.example.com/" : "DIRECT",
      "http://slashdot.org/" : "PROXY httpproxy.com:8080",
    }
  },
  {
    "name": "FtpNonProxyHosts",
    "description" : "Test that FTP non proxy hosts are mapped correctly",
    "properties" : {
      "ftp.nonProxyHosts" : "slashdot.org",
      "ftp.proxyHost" : "httpproxy.com",
      "ftp.proxyPort" : "8080",
    },
    "mappings" : {
      "http://example.com/" : "DIRECT",
      "ftp://example.com/" : "PROXY httpproxy.com:8080",
    }
  },
  {
    "name": "FtpProxyHostAndPort",
    "description" : "Test ftp.proxyHost and ftp.proxyPort works.",
    "properties" : {
      "ftp.proxyHost" : "httpproxy.com",
      "ftp.proxyPort" : "8080",
    },
    "mappings" : {
      "ftp://example.com/" : "PROXY httpproxy.com:8080",
      "http://example.com/" : "DIRECT",
      "https://example.com/" : "DIRECT",
    }
  },
  {
    "name": "FtpProxyHostOnly",
    "description" : "Test ftp.proxyHost and default port.",
    "properties" : {
      "ftp.proxyHost" : "httpproxy.com",
    },
    "mappings" : {
      "ftp://example.com/" : "PROXY httpproxy.com:80",
      "http://example.com/" : "DIRECT",
      "https://example.com/" : "DIRECT",
    }
  },
  {
    "name": "HttpsProxyHostAndPort",
    "description" : "Test https.proxyHost and https.proxyPort works.",
    "properties" : {
      "https.proxyHost" : "httpproxy.com",
      "https.proxyPort" : "8080",
    },
    "mappings" : {
      "https://example.com/" : "PROXY httpproxy.com:8080",
      "http://example.com/" : "DIRECT",
      "ftp://example.com/" : "DIRECT",
    }
  },
  {
    "name": "HttpsProxyHostOnly",
    "description" : "Test https.proxyHost and default port.",
    # Chromium differs from the Android platform by connecting to port 80 for
    # HTTPS connections by default, hence cpp-only.
    "cpp-only" : "",
    "properties" : {
      "https.proxyHost" : "httpproxy.com",
    },
    "mappings" : {
      "https://example.com/" : "PROXY httpproxy.com:80",
      "http://example.com/" : "DIRECT",
      "ftp://example.com/" : "DIRECT",
    }
  },
  {
    "name": "HttpProxyHostIPv6",
    "description" : "Test IPv6 https.proxyHost and default port.",
    "cpp-only" : "",
    "properties" : {
      "http.proxyHost" : "a:b:c::d:1",
    },
    "mappings" : {
      "http://example.com/" : "PROXY [a:b:c::d:1]:80",
      "ftp://example.com/" : "DIRECT",
    }
  },
  {
    "name": "HttpProxyHostAndPortIPv6",
    "description" : "Test IPv6 http.proxyHost and http.proxyPort works.",
    "cpp-only" : "",
    "properties" : {
      "http.proxyHost" : "a:b:c::d:1",
      "http.proxyPort" : "8080",
    },
    "mappings" : {
      "http://example.com/" : "PROXY [a:b:c::d:1]:8080",
      "ftp://example.com/" : "DIRECT",
    }
  },
  {
    "name": "HttpProxyHostAndInvalidPort",
    "description" : "Test invalid http.proxyPort does not crash.",
    "cpp-only" : "",
    "properties" : {
      "http.proxyHost" : "a:b:c::d:1",
      "http.proxyPort" : "65536",
    },
    "mappings" : {
      "http://example.com/" : "DIRECT",
      "ftp://example.com/" : "DIRECT",
    }
  },
  {
    "name": "DefaultProxyExplictPort",
    "description" :
        "Default http proxy is used if a scheme-specific one is not found.",
    "properties" : {
      "proxyHost" : "defaultproxy.com",
      "proxyPort" : "8080",
      "ftp.proxyHost" : "httpproxy.com",
      "ftp.proxyPort" : "8080",
    },
    "mappings" : {
      "http://example.com/" : "PROXY defaultproxy.com:8080",
      "https://example.com/" : "PROXY defaultproxy.com:8080",
      "ftp://example.com/" : "PROXY httpproxy.com:8080",
    }
  },
  {
    "name": "DefaultProxyDefaultPort",
    "description" : "Check that the default proxy port is as expected.",
    # Chromium differs from the Android platform by connecting to port 80 for
    # HTTPS connections by default, hence cpp-only.
    "cpp-only" : "",
    "properties" : {
      "proxyHost" : "defaultproxy.com",
    },
    "mappings" : {
      "http://example.com/" : "PROXY defaultproxy.com:80",
      "https://example.com/" : "PROXY defaultproxy.com:80",
    }
  },
  {
    "name": "FallbackToSocks",
    "description" : "SOCKS proxy is used if scheme-specific one is not found.",
    "properties" : {
      "http.proxyHost" : "defaultproxy.com",
      "socksProxyHost" : "socksproxy.com"
    },
    "mappings" : {
      "http://example.com/" : "PROXY defaultproxy.com:80",
      "https://example.com/" : "SOCKS5 socksproxy.com:1080",
      "ftp://example.com" : "SOCKS5 socksproxy.com:1080",
    }
  },
  {
    "name": "SocksExplicitPort",
    "description" : "SOCKS proxy port is used if specified",
    "properties" : {
      "socksProxyHost" : "socksproxy.com",
      "socksProxyPort" : "9000",
    },
    "mappings" : {
      "http://example.com/" : "SOCKS5 socksproxy.com:9000",
    }
  },
  {
    "name": "HttpProxySupercedesSocks",
    "description" : "SOCKS proxy is ignored if default HTTP proxy defined.",
    "properties" : {
      "proxyHost" : "defaultproxy.com",
      "socksProxyHost" : "socksproxy.com",
      "socksProxyPort" : "9000",
    },
    "mappings" : {
      "http://example.com/" : "PROXY defaultproxy.com:80",
    }
  },
]

class GenerateCPlusPlus:
  """Generate C++ test cases"""

  def Generate(self):
    for test_case in test_cases:
      print ("TEST_F(ProxyConfigServiceAndroidTest, %s) {" % test_case["name"])
      if "description" in test_case:
        self._GenerateDescription(test_case["description"]);
      self._GenerateConfiguration(test_case["properties"])
      self._GenerateMappings(test_case["mappings"])
      print "}"
      print ""

  def _GenerateDescription(self, description):
    print "  // %s" % description

  def _GenerateConfiguration(self, properties):
    for key in sorted(properties.iterkeys()):
      print "  AddProperty(\"%s\", \"%s\");" % (key, properties[key])
    print "  ProxySettingsChanged();"

  def _GenerateMappings(self, mappings):
    for url in sorted(mappings.iterkeys()):
      print "  TestMapping(\"%s\", \"%s\");" % (url, mappings[url])


class GenerateJava:
  """Generate Java test cases"""

  def Generate(self):
    for test_case in test_cases:
      if "cpp-only" in test_case:
        continue
      if "description" in test_case:
        self._GenerateDescription(test_case["description"]);
      print "    @SmallTest"
      print "    @Feature({\"AndroidWebView\"})"
      print "    public void test%s() throws Exception {" % test_case["name"]
      self._GenerateConfiguration(test_case["properties"])
      self._GenerateMappings(test_case["mappings"])
      print "    }"
      print ""

  def _GenerateDescription(self, description):
    print "    /**"
    print "     * %s" % description
    print "     *"
    print "     * @throws Exception"
    print "     */"

  def _GenerateConfiguration(self, properties):
    for key in sorted(properties.iterkeys()):
      print "        System.setProperty(\"%s\", \"%s\");" % (
          key, properties[key])

  def _GenerateMappings(self, mappings):
    for url in sorted(mappings.iterkeys()):
      mapping = mappings[url]
      if 'HTTPS' in mapping:
        mapping = mapping.replace('HTTPS', 'PROXY')
      print "        checkMapping(\"%s\", \"%s\");" % (url, mapping)


def main():
  parser = optparse.OptionParser()
  parser.add_option("-j", "--java",
                action="store_true", dest="java");
  (options, args) = parser.parse_args();
  if options.java:
    generator = GenerateJava()
  else:
    generator = GenerateCPlusPlus()
  generator.Generate()

if __name__ == '__main__':
  main()
