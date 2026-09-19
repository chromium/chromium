// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.url.URI;

import java.net.URISyntaxException;
import java.net.URLDecoder;
import java.nio.charset.StandardCharsets;

@RunWith(BaseRobolectricTestRunner.class)
public class EncodeHtmlDataUriUnitTest {
    private static final String DATA_URI_PREFIX = "data:text/html;utf-8,";

    private String getData(String dataUri) {
        Assert.assertNotNull("Data URI is null", dataUri);
        Assert.assertTrue("Incorrect HTML Data URI prefix", dataUri.startsWith(DATA_URI_PREFIX));
        return dataUri.substring(DATA_URI_PREFIX.length());
    }

    private String decode(String dataUri) {
        String data = getData(dataUri);
        return URLDecoder.decode(data, StandardCharsets.UTF_8);
    }

    @Test
    public void testDelimitersEncoding() {
        String testString = "><#%\"'";
        String encodedUri = UrlUtils.encodeHtmlDataUri(testString);
        String decodedUri = decode(encodedUri);
        Assert.assertEquals("Delimiters are not properly encoded", decodedUri, testString);
    }

    @Test
    public void testUnwiseCharactersEncoding() {
        String testString = "{}|\\^[]`";
        String encodedUri = UrlUtils.encodeHtmlDataUri(testString);
        String decodedUri = decode(encodedUri);
        Assert.assertEquals("Unwise characters are not properly encoded", decodedUri, testString);
    }

    @Test
    public void testWhitespaceEncoding() {
        String testString = " \n\t";
        String encodedUri = UrlUtils.encodeHtmlDataUri(testString);
        String decodedUri = decode(encodedUri);
        Assert.assertEquals(
                "Whitespace characters are not properly encoded", decodedUri, testString);
    }

    @Test
    public void testReturnsValidUri() throws URISyntaxException {
        String testString = "<html><body onload=\"alert('Hello \\\"world\\\"');\"></body></html>";
        String encodedUri = UrlUtils.encodeHtmlDataUri(testString);
        String decodedUri = decode(encodedUri);
        // Verify that the encoded URI is valid.
        new URI(encodedUri);
        // Verify that something sensible was encoded.
        Assert.assertEquals("Simple HTML is not properly encoded", decodedUri, testString);
    }
}
