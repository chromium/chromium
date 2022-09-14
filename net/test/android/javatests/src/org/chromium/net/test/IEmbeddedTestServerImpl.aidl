// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.test;

import org.chromium.net.test.IConnectionListener;

interface IEmbeddedTestServerImpl {

    /** Initialize the native object. */
    boolean initializeNative(boolean https);

    /** Start the server.
     *
     *  @param port The port to use for the server, 0 to auto-select an unused port.
     *  @return Whether the server was successfully started.
     */
    boolean start(int port);

    /** Get the path of the server's root certificate.
     *
     *  @return The pathname of a PEM file containing the server's root certificate.
     */
    String getRootCertPemPath();

    /** Add the default handlers and serve files from the provided directory relative to the
     *  external storage directory.
     *
     *  @param directoryPath The path of the directory from which files should be served, relative
     *      to the external storage directory.
     */
    void addDefaultHandlers(String directoryPath);

    /** Configure the server to use a particular type of SSL certificate.
     *
     * @param serverCertificate The type of certificate the server should use.
     */
    void setSSLConfig(int serverCertificate);

    /** Serve files from the provided directory.
     *
     *  @param directoryPath The path of the directory from which files should be served.
     */
    void serveFilesFromDirectory(String directoryPath);

    /** Get the full URL for the given relative URL.
     *
     *  @param relativeUrl The relative URL for which a full URL should be returned.
     *  @return The URL as a String.
     */
    String getURL(String relativeUrl);

    /** Get the full URL for the given relative URL. Similar to the above method but uses the given
     *  hostname instead of 127.0.0.1. The hostname should be resolved to 127.0.0.1.
     *
     *  @param hostName The host name which should be used.
     *  @param relativeUrl The relative URL for which a full URL should be returned.
     *  @return The URL as a String.
     */
    String getURLWithHostName(String hostName, String relativeUrl);

    /** Shut down the server.
     *
     *  @return Whether the server was successfully shut down.
     */
    boolean shutdownAndWaitUntilComplete();

    /** Destroy the native object. */
    void destroy();

    /** Set a connection listener. Must be called before {@link start()}. */
    void setConnectionListener(IConnectionListener callback);
}
