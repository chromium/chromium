// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.url.URI;

import java.net.URLDecoder;

@RunWith(BaseJUnit4ClassRunner.class)
public class EncodeHtmlDataUriTest {
    private static final String DATA_URI_PREFIX = "data:text/html;utf-8,";

    private String getData(String dataUri) {
        Assert.assertNotNull("Data URI is null", dataUri);
        Assert.assertTrue("Incorrect HTML Data URI prefix", dataUri.startsWith(DATA_URI_PREFIX));
        return dataUri.substring(DATA_URI_PREFIX.length());
    }

    private String decode(String dataUri) throws java.io.UnsupportedEncodingException {
        String data = getData(dataUri);
        return URLDecoder.decode(data, "UTF-8");
    }

    @Test
    @SmallTest
    public void testDelimitersEncoding() throws java.io.UnsupportedEncodingException {
        String testString = "><#%\"'";
        String encodedUri = UrlUtils.encodeHtmlDataUri(testString);
        String decodedUri = decode(encodedUri);
        Assert.assertEquals("Delimiters are not properly encoded", decodedUri, testString);
    }

    @Test
    @SmallTest
    public void testUnwiseCharactersEncoding() throws java.io.UnsupportedEncodingException {
        String testString = "{}|\\^[]`";
        String encodedUri = UrlUtils.encodeHtmlDataUri(testString);
        String decodedUri = decode(encodedUri);
        Assert.assertEquals("Unwise characters are not properly encoded", decodedUri, testString);
    }

    @Test
    @SmallTest
    public void testWhitespaceEncoding() throws java.io.UnsupportedEncodingException {
        String testString = " \n\t";
        String encodedUri = UrlUtils.encodeHtmlDataUri(testString);
        String decodedUri = decode(encodedUri);
        Assert.assertEquals(
                "Whitespace characters are not properly encoded", decodedUri, testString);
    }

    @Test
    @SmallTest
    public void testReturnsValidUri()
            throws java.net.URISyntaxException, java.io.UnsupportedEncodingException {
        String testString = "<html><body onload=\"alert('Hello \\\"world\\\"');\"></body></html>";
        String encodedUri = UrlUtils.encodeHtmlDataUri(testString);
        String decodedUri = decode(encodedUri);
        // Verify that the encoded URI is valid.
        new URI(encodedUri);
        // Verify that something sensible was encoded.
        Assert.assertEquals("Simple HTML is not properly encoded", decodedUri, testString);
    }
}
