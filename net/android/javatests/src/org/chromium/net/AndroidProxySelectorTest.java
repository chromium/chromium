// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Test suite for Android's default ProxySelector implementation. The purpose of these tests
 * is to check that the behaviour of the ProxySelector implementation matches what we have
 * implemented in net/proxy_resolution/proxy_config_service_android.cc.
 *
 * IMPORTANT: These test cases are generated from net/android/tools/proxy_test_cases.py, so if any
 * of these tests fail, please be sure to edit that file and regenerate the test cases here and also
 * in net/proxy_resolution/proxy_config_service_android_unittests.cc if required.
 */
package org.chromium.net;

import android.os.Build;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;

import java.net.Proxy;
import java.net.ProxySelector;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.List;
import java.util.Properties;

@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class AndroidProxySelectorTest {
    Properties mProperties;

    public AndroidProxySelectorTest() {
        // Start with a clean slate in case there is a system proxy configured.
        mProperties = new Properties();
    }

    @Before
    public void setUp() {
        System.setProperties(mProperties);
    }

    @After
    public void tearDown() {
        System.setProperties(mProperties);
    }

    static String toString(Proxy proxy) {
        if (proxy.equals(Proxy.NO_PROXY)) return "DIRECT";
        // java.net.Proxy only knows about http and socks proxies.
        Proxy.Type type = proxy.type();
        switch (type) {
            case HTTP:
                return "PROXY " + proxy.address().toString();
            case SOCKS:
                return "SOCKS5 " + proxy.address().toString();
            case DIRECT:
                return "DIRECT";
            default:
                // If a new proxy type is supported in future, add a case to match it.
                Assert.fail("Unknown proxy type" + type);
                return "unknown://";
        }
    }

    static String toString(List<Proxy> proxies) {
        StringBuilder builder = new StringBuilder();
        for (Proxy proxy : proxies) {
            if (builder.length() > 0) builder.append(';');
            builder.append(toString(proxy));
        }
        return builder.toString();
    }

    static void checkMapping(String url, String expected) throws URISyntaxException {
        URI uri = new URI(url);
        List<Proxy> proxies = ProxySelector.getDefault().select(uri);
        Assert.assertEquals("Mapping", expected, toString(proxies));
    }

    /** Test direct mapping when no proxy defined. */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testNoProxy() throws Exception {
        checkMapping("ftp://example.com/", "DIRECT");
        checkMapping("http://example.com/", "DIRECT");
        checkMapping("https://example.com/", "DIRECT");
    }

    /** Test http.proxyHost and http.proxyPort works. */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testHttpProxyHostAndPort() throws Exception {
        System.setProperty("http.proxyHost", "httpproxy.com");
        System.setProperty("http.proxyPort", "8080");
        checkMapping("ftp://example.com/", "DIRECT");
        checkMapping("http://example.com/", "PROXY httpproxy.com:8080");
        checkMapping("https://example.com/", "DIRECT");
    }

    /** We should get the default port (80) for proxied hosts. */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testHttpProxyHostOnly() throws Exception {
        System.setProperty("http.proxyHost", "httpproxy.com");
        checkMapping("ftp://example.com/", "DIRECT");
        checkMapping("http://example.com/", "PROXY httpproxy.com:80");
        checkMapping("https://example.com/", "DIRECT");
    }

    /** http.proxyPort only should not result in any hosts being proxied. */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testHttpProxyPortOnly() throws Exception {
        System.setProperty("http.proxyPort", "8080");
        checkMapping("ftp://example.com/", "DIRECT");
        checkMapping("http://example.com/", "DIRECT");
        checkMapping("https://example.com/", "DIRECT");
    }

    /** Test that HTTP non proxy hosts are mapped correctly */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testHttpNonProxyHosts1() throws Exception {
        System.setProperty("http.nonProxyHosts", "slashdot.org");
        System.setProperty("http.proxyHost", "httpproxy.com");
        System.setProperty("http.proxyPort", "8080");
        checkMapping("http://example.com/", "PROXY httpproxy.com:8080");
        checkMapping("http://slashdot.org/", "DIRECT");
    }

    /** Test that | pattern works. */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testHttpNonProxyHosts2() throws Exception {
        System.setProperty("http.nonProxyHosts", "slashdot.org|freecode.net");
        System.setProperty("http.proxyHost", "httpproxy.com");
        System.setProperty("http.proxyPort", "8080");
        checkMapping("http://example.com/", "PROXY httpproxy.com:8080");
        checkMapping("http://freecode.net/", "DIRECT");
        checkMapping("http://slashdot.org/", "DIRECT");
    }

    /** Test that * pattern works. */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testHttpNonProxyHosts3() throws Exception {
        System.setProperty("http.nonProxyHosts", "*example.com");
        System.setProperty("http.proxyHost", "httpproxy.com");
        System.setProperty("http.proxyPort", "8080");
        // TODO(jbudorick): Find an appropriate upper bound for this. crbug.com/726360
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N) {
            checkMapping("http://example.com/", "DIRECT");
        }
        checkMapping("http://slashdot.org/", "PROXY httpproxy.com:8080");
        checkMapping("http://www.example.com/", "DIRECT");
    }

    /** Test that FTP non proxy hosts are mapped correctly */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testFtpNonProxyHosts() throws Exception {
        System.setProperty("ftp.nonProxyHosts", "slashdot.org");
        System.setProperty("ftp.proxyHost", "httpproxy.com");
        System.setProperty("ftp.proxyPort", "8080");
        checkMapping("ftp://example.com/", "PROXY httpproxy.com:8080");
        checkMapping("http://example.com/", "DIRECT");
    }

    /** Test ftp.proxyHost and ftp.proxyPort works. */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testFtpProxyHostAndPort() throws Exception {
        System.setProperty("ftp.proxyHost", "httpproxy.com");
        System.setProperty("ftp.proxyPort", "8080");
        checkMapping("ftp://example.com/", "PROXY httpproxy.com:8080");
        checkMapping("http://example.com/", "DIRECT");
        checkMapping("https://example.com/", "DIRECT");
    }

    /** Test ftp.proxyHost and default port. */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testFtpProxyHostOnly() throws Exception {
        System.setProperty("ftp.proxyHost", "httpproxy.com");
        checkMapping("ftp://example.com/", "PROXY httpproxy.com:80");
        checkMapping("http://example.com/", "DIRECT");
        checkMapping("https://example.com/", "DIRECT");
    }

    /** Test https.proxyHost and https.proxyPort works. */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testHttpsProxyHostAndPort() throws Exception {
        System.setProperty("https.proxyHost", "httpproxy.com");
        System.setProperty("https.proxyPort", "8080");
        checkMapping("ftp://example.com/", "DIRECT");
        checkMapping("http://example.com/", "DIRECT");
        checkMapping("https://example.com/", "PROXY httpproxy.com:8080");
    }

    /** Default http proxy is used if a scheme-specific one is not found. */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDefaultProxyExplictPort() throws Exception {
        System.setProperty("ftp.proxyHost", "httpproxy.com");
        System.setProperty("ftp.proxyPort", "8080");
        System.setProperty("proxyHost", "defaultproxy.com");
        System.setProperty("proxyPort", "8080");
        checkMapping("ftp://example.com/", "PROXY httpproxy.com:8080");
        checkMapping("http://example.com/", "PROXY defaultproxy.com:8080");
        checkMapping("https://example.com/", "PROXY defaultproxy.com:8080");
    }

    /** SOCKS proxy is used if scheme-specific one is not found. */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testFallbackToSocks() throws Exception {
        System.setProperty("http.proxyHost", "defaultproxy.com");
        System.setProperty("socksProxyHost", "socksproxy.com");
        checkMapping("ftp://example.com", "SOCKS5 socksproxy.com:1080");
        checkMapping("http://example.com/", "PROXY defaultproxy.com:80");
        checkMapping("https://example.com/", "SOCKS5 socksproxy.com:1080");
    }

    /** SOCKS proxy port is used if specified */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSocksExplicitPort() throws Exception {
        System.setProperty("socksProxyHost", "socksproxy.com");
        System.setProperty("socksProxyPort", "9000");
        checkMapping("http://example.com/", "SOCKS5 socksproxy.com:9000");
    }

    /** SOCKS proxy is ignored if default HTTP proxy defined. */
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testHttpProxySupercedesSocks() throws Exception {
        System.setProperty("proxyHost", "defaultproxy.com");
        System.setProperty("socksProxyHost", "socksproxy.com");
        System.setProperty("socksProxyPort", "9000");
        checkMapping("http://example.com/", "PROXY defaultproxy.com:80");
    }
}
