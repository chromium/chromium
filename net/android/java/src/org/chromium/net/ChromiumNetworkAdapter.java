// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import java.io.IOException;
import java.io.InputStream;
import java.net.Proxy;
import java.net.URL;
import java.net.URLConnection;

/** Wrapper class for network requests. */
public final class ChromiumNetworkAdapter {
    private ChromiumNetworkAdapter() {}

    /**
     * Wrapper around URL#openConnection(), with an extra argument for static analysis/privacy
     * auditing.
     *
     * @param url the URL to open connection to.
     * @param trafficAnnotation an object documenting this network request: what it's used for, what
     *     data gets sent, what triggers it, etc.
     * @return a URLConnection linking to the URL.
     */
    public static URLConnection openConnection(
            URL url, NetworkTrafficAnnotationTag trafficAnnotation) throws IOException {
        return url.openConnection();
    }

    /**
     * Wrapper around URL#openConnection(Proxy), with an extra argument for static analysis/privacy
     * auditing.
     *
     * @param url the URL to open connection to.
     * @param proxy the Proxy through which this connection will be made.
     * @param trafficAnnotation an object documenting this network request: what it's used for, what
     *     data gets sent, what triggers it, etc.
     * @return a URLConnection linking to the URL.
     */
    public static URLConnection openConnection(
            URL url, Proxy proxy, NetworkTrafficAnnotationTag trafficAnnotation)
            throws IOException {
        return url.openConnection(proxy);
    }

    /**
     * Wrapper around URL#openStream(), with an extra argument for static analysis/privacy auditing.
     *
     * @param url the URL to open connection to.
     * @param trafficAnnotation an object documenting this network request: what it's used for, what
     *     data gets sent, what triggers it, etc.
     * @return an InputStream linking to the URL.
     */
    public static InputStream openStream(URL url, NetworkTrafficAnnotationTag trafficAnnotation)
            throws IOException {
        return url.openStream();
    }
}
