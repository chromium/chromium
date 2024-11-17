// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.test.util;

import android.util.Base64;

import androidx.annotation.GuardedBy;

import org.chromium.base.Log;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.UnsupportedEncodingException;
import java.net.MalformedURLException;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.SocketException;
import java.security.KeyStore;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import javax.net.ssl.KeyManager;
import javax.net.ssl.KeyManagerFactory;
import javax.net.ssl.SSLContext;

/**
 * Simple http test server for testing.
 *
 * This server runs in a thread in the current process, so it is convenient
 * for loopback testing without the need to setup TCP forwarding to the
 * host computer.
 */
public class WebServer implements AutoCloseable {
    private static final String TAG = "WebServer";

    private static Set<WebServer> sInstances = new HashSet<>();
    private static Set<WebServer> sSecureInstances = new HashSet<>();

    private final ServerThread mServerThread;
    private String mServerUri;
    private final boolean mSsl;
    private final int mPort;

    public static final String STATUS_OK = "200 OK";

    /**
     * Writes an HTTP response to |output|.
     * |status| should be one of the STATUS_* values above.
     */
    public static void writeResponse(OutputStream output, String status, byte[] body)
            throws IOException {
        if (body == null) {
            body = new byte[0];
        }
        output.write(
                String.format("HTTP/1.1 %s\r\nContent-Length: %s\r\n\r\n", status, body.length)
                        .getBytes());
        output.write(body);
        output.flush();
    }

    /** Represents an HTTP header. */
    public static class HTTPHeader {
        public final String key;
        public final String value;

        /** Constructs an HTTP header. */
        public HTTPHeader(String key, String value) {
            this.key = key;
            this.value = value;
        }

        /**
         * Parse an HTTP header from a string line. Returns null if the line is not a valid HTTP
         * header.
         */
        public static HTTPHeader parseLine(String line) {
            String[] parts = line.split(":", 2);
            if (parts.length == 2) {
                return new HTTPHeader(parts[0].trim(), parts[1].trim());
            }
            return null;
        }

        @Override
        public String toString() {
            return key + ": " + value;
        }
    }

    /** Thrown when an HTTP request could not be parsed. */
    public static class InvalidRequest extends Exception {
        /** Constructor */
        public InvalidRequest() {
            super("Invalid HTTP request");
        }
    }

    /** A parsed HTTP request. */
    public static class HTTPRequest {
        private String mMethod;
        private String mURI;
        private String mHTTPVersion;
        private HTTPHeader[] mHeaders;
        private byte[] mBody;

        @Override
        public String toString() {
            StringBuilder builder = new StringBuilder();
            builder.append(requestLine());
            builder.append("\r\n");
            for (HTTPHeader header : mHeaders) {
                builder.append(header.toString());
                builder.append("\r\n");
            }
            if (mBody != null) {
                builder.append("\r\n");
                try {
                    builder.append(new String(mBody, "UTF-8"));
                } catch (UnsupportedEncodingException e) {
                    builder.append("<binary body, length=").append(mBody.length).append(">\r\n");
                }
            }
            return builder.toString();
        }

        /** Returns the request line as a String. */
        public String requestLine() {
            return mMethod + " " + mURI + " " + mHTTPVersion;
        }

        /** Returns the request method. */
        public String getMethod() {
            return mMethod;
        }

        /** Returns the request URI. */
        public String getURI() {
            return mURI;
        }

        /** Returns the request HTTP version. */
        public String getHTTPVersion() {
            return mHTTPVersion;
        }

        /** Returns the request headers. */
        public HTTPHeader[] getHeaders() {
            return mHeaders;
        }

        /** Returns the request body. */
        public byte[] getBody() {
            return mBody;
        }

        /**
         * Returns the header value for the given header name. If a header is present multiple
         * times, this only returns the first occurence. Returns "" if the header is not found.
         */
        public String headerValue(String headerName) {
            for (String value : headerValues(headerName)) {
                return value;
            }
            return "";
        }

        /** Returns all header values for the given header name. */
        public List<String> headerValues(String headerName) {
            List<String> matchingHeaders = new ArrayList<String>();
            for (HTTPHeader header : mHeaders) {
                if (header.key.equalsIgnoreCase(headerName)) {
                    matchingHeaders.add(header.value);
                }
            }
            return matchingHeaders;
        }

        private static boolean hasChunkedTransferEncoding(HTTPRequest req) {
            List<String> transferEncodings = req.headerValues("Transfer-Encoding");
            for (String encoding : transferEncodings) {
                if (encoding.equals("chunked")) {
                    return true;
                }
            }
            return false;
        }

        /** Parses an HTTP request from an input stream. */
        public static HTTPRequest parse(InputStream stream) throws InvalidRequest, IOException {
            boolean firstLine = true;
            HTTPRequest req = new HTTPRequest();
            ArrayList<HTTPHeader> mHeaders = new ArrayList<HTTPHeader>();
            ByteArrayOutputStream line = new ByteArrayOutputStream();
            for (int b = stream.read(); b != -1; b = stream.read()) {
                if (b == '\r') {
                    int next = stream.read();
                    if (next == '\n') {
                        String lineString;
                        try {
                            lineString = new String(line.toByteArray(), "UTF-8");
                        } catch (UnsupportedEncodingException e) {
                            throw new InvalidRequest();
                        }
                        line.reset();
                        if (firstLine) {
                            String[] parts = lineString.split(" ", 3);
                            if (parts.length != 3) {
                                throw new InvalidRequest();
                            }
                            req.mMethod = parts[0];
                            req.mURI = parts[1];
                            req.mHTTPVersion = parts[2];
                            firstLine = false;
                        } else {
                            if (lineString.length() == 0) {
                                break;
                            }
                            HTTPHeader header = HTTPHeader.parseLine(lineString);
                            if (header != null) {
                                mHeaders.add(header);
                            }
                        }
                    } else if (next == -1) {
                        throw new InvalidRequest();
                    } else {
                        line.write(b);
                        line.write(next);
                    }
                } else {
                    line.write(b);
                }
            }
            if (firstLine) {
                if (line.size() == 0) return null;
                throw new InvalidRequest();
            }
            req.mHeaders = mHeaders.toArray(new HTTPHeader[0]);
            int contentLength = -1;
            if (req.mMethod.equals("GET") || req.mMethod.equals("HEAD")) {
                contentLength = 0;
            }
            try {
                contentLength = Integer.parseInt(req.headerValue("Content-Length"));
            } catch (NumberFormatException e) {
            }
            if (contentLength >= 0) {
                byte[] content = new byte[contentLength];
                for (int offset = 0; offset < contentLength; ) {
                    int bytesRead = stream.read(content, offset, contentLength);
                    if (bytesRead == -1) { // short read, keep truncated content.
                        content = Arrays.copyOf(content, offset);
                        break;
                    }
                    offset += bytesRead;
                }
                req.mBody = content;
            } else if (hasChunkedTransferEncoding(req)) {
                ByteArrayOutputStream mBody = new ByteArrayOutputStream();
                byte[] buffer = new byte[1000];
                int bytesRead;
                while ((bytesRead = stream.read(buffer, 0, buffer.length)) != -1) {
                    mBody.write(buffer, 0, bytesRead);
                }
                req.mBody = mBody.toByteArray();
            }
            return req;
        }
    }

    /** An interface for handling HTTP requests. */
    public interface RequestHandler {
        /** handleRequest is called when an HTTP request is received. handleRequest should write a
         * response to stream. */
        void handleRequest(HTTPRequest request, OutputStream stream);
    }

    private RequestHandler mRequestHandler;

    /** Sets the request handler. */
    public void setRequestHandler(RequestHandler handler) {
        mRequestHandler = handler;
    }

    /** Handle an HTTP request. Calls |mRequestHandler| if set. */
    private void handleRequest(HTTPRequest request, OutputStream stream) {
        assert Thread.currentThread() == mServerThread
                : "handleRequest called from non-server thread";
        if (mRequestHandler != null) {
            mRequestHandler.handleRequest(request, stream);
        }
    }

    public void setServerHost(String hostname) {
        try {
            mServerUri =
                    new java.net.URI(
                                    mSsl ? "https" : "http",
                                    null,
                                    hostname,
                                    mServerThread.mSocket.getLocalPort(),
                                    null,
                                    null,
                                    null)
                            .toString();
        } catch (java.net.URISyntaxException e) {
            Log.wtf(TAG, e.getMessage());
        }
    }

    /**
     * Create and start a local HTTP server instance. Additional must only be true if an instance
     * was already created. You are responsible for calling shutdown() on each instance you create.
     *
     * @param port Port number the server must use, or 0 to automatically choose a free port.
     * @param ssl True if the server should be using secure sockets.
     * @param additional True if creating an additional server instance.
     */
    public WebServer(int port, boolean ssl, boolean additional) throws Exception {
        mPort = port;
        mSsl = ssl;

        if (mSsl) {
            if ((additional && WebServer.sSecureInstances.isEmpty())
                    || (!additional && !WebServer.sSecureInstances.isEmpty())) {
                throw new IllegalStateException(
                        "There are "
                                + WebServer.sSecureInstances.size()
                                + " SSL WebServer instances. Expected "
                                + (additional ? ">=1" : "0")
                                + " because additional is "
                                + additional);
            }
        } else {
            if ((additional && WebServer.sInstances.isEmpty())
                    || (!additional && !WebServer.sInstances.isEmpty())) {
                throw new IllegalStateException(
                        "There are "
                                + WebServer.sSecureInstances.size()
                                + " WebServer instances. Expected "
                                + (additional ? ">=1" : "0")
                                + " because additional is "
                                + additional);
            }
        }
        mServerThread = new ServerThread(mPort, mSsl);

        setServerHost("localhost");

        mServerThread.start();
        if (mSsl) {
            WebServer.sSecureInstances.add(this);
        } else {
            WebServer.sInstances.add(this);
        }
    }

    /**
     * Create and start a local HTTP server instance.
     *
     * @param port Port number the server must use, or 0 to automatically choose a free port.
     * @param ssl True if the server should be using secure sockets.
     */
    public WebServer(int port, boolean ssl) throws Exception {
        this(port, ssl, false);
    }

    /** Terminate the http server. */
    public void shutdown() {
        if (mSsl) {
            WebServer.sSecureInstances.remove(this);
        } else {
            WebServer.sInstances.remove(this);
        }

        try {
            mServerThread.cancelAllRequests();
            // Block until the server thread is done shutting down.
            mServerThread.join();
        } catch (MalformedURLException e) {
            throw new IllegalStateException(e);
        } catch (InterruptedException | IOException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Make the WebServer AutoCloseable.
     * Calls the shutdown method.
     */
    @Override
    public void close() {
        shutdown();
    }

    public String getBaseUrl() {
        return mServerUri + "/";
    }

    /**
     * Gets the URL on the server under which a particular request path will be accessible.
     *
     * This only gets the URL, you still need to set the response if you intend to access it.
     *
     * @param requestPath The path to respond to.
     * @return The full URL including the requestPath.
     */
    public String getResponseUrl(String requestPath) {
        return mServerUri + requestPath;
    }

    private class ServerThread extends Thread {
        private final boolean mIsSsl;
        private ServerSocket mSocket;
        private SSLContext mSslContext;

        private final Object mLock = new Object();

        @GuardedBy("mLock")
        private boolean mIsCancelled;

        @GuardedBy("mLock")
        private Socket mCurrentRequestSocket;

        /**
         * Defines the keystore contents for the server, BKS version. Holds just a single
         * self-generated key. The subject name is "Test Server".
         */
        private static final String SERVER_KEYS_BKS =
                "AAAAAQAAABQDkebzoP1XwqyWKRCJEpn/t8dqIQAABDkEAAVteWtleQAAARpYl20nAAAAAQAFWC41"
                    + "MDkAAAJNMIICSTCCAbKgAwIBAgIESEfU1jANBgkqhkiG9w0BAQUFADBpMQswCQYDVQQGEwJVUzET"
                    + "MBEGA1UECBMKQ2FsaWZvcm5pYTEMMAoGA1UEBxMDTVRWMQ8wDQYDVQQKEwZHb29nbGUxEDAOBgNV"
                    + "BAsTB0FuZHJvaWQxFDASBgNVBAMTC1Rlc3QgU2VydmVyMB4XDTA4MDYwNTExNTgxNFoXDTA4MDkw"
                    + "MzExNTgxNFowaTELMAkGA1UEBhMCVVMxEzARBgNVBAgTCkNhbGlmb3JuaWExDDAKBgNVBAcTA01U"
                    + "VjEPMA0GA1UEChMGR29vZ2xlMRAwDgYDVQQLEwdBbmRyb2lkMRQwEgYDVQQDEwtUZXN0IFNlcnZl"
                    + "cjCBnzANBgkqhkiG9w0BAQEFAAOBjQAwgYkCgYEA0LIdKaIr9/vsTq8BZlA3R+NFWRaH4lGsTAQy"
                    + "DPMF9ZqEDOaL6DJuu0colSBBBQ85hQTPa9m9nyJoN3pEi1hgamqOvQIWcXBk+SOpUGRZZFXwniJV"
                    + "zDKU5nE9MYgn2B9AoiH3CSuMz6HRqgVaqtppIe1jhukMc/kHVJvlKRNy9XMCAwEAATANBgkqhkiG"
                    + "9w0BAQUFAAOBgQC7yBmJ9O/eWDGtSH9BH0R3dh2NdST3W9hNZ8hIa8U8klhNHbUCSSktZmZkvbPU"
                    + "hse5LI3dh6RyNDuqDrbYwcqzKbFJaq/jX9kCoeb3vgbQElMRX8D2ID1vRjxwlALFISrtaN4VpWzV"
                    + "yeoHPW4xldeZmoVtjn8zXNzQhLuBqX2MmAAAAqwAAAAUvkUScfw9yCSmALruURNmtBai7kQAAAZx"
                    + "4Jmijxs/l8EBaleaUru6EOPioWkUAEVWCxjM/TxbGHOi2VMsQWqRr/DZ3wsDmtQgw3QTrUK666sR"
                    + "MBnbqdnyCyvM1J2V1xxLXPUeRBmR2CXorYGF9Dye7NkgVdfA+9g9L/0Au6Ugn+2Cj5leoIgkgApN"
                    + "vuEcZegFlNOUPVEs3SlBgUF1BY6OBM0UBHTPwGGxFBBcetcuMRbUnu65vyDG0pslT59qpaR0TMVs"
                    + "P+tcheEzhyjbfM32/vwhnL9dBEgM8qMt0sqF6itNOQU/F4WGkK2Cm2v4CYEyKYw325fEhzTXosck"
                    + "MhbqmcyLab8EPceWF3dweoUT76+jEZx8lV2dapR+CmczQI43tV9btsd1xiBbBHAKvymm9Ep9bPzM"
                    + "J0MQi+OtURL9Lxke/70/MRueqbPeUlOaGvANTmXQD2OnW7PISwJ9lpeLfTG0LcqkoqkbtLKQLYHI"
                    + "rQfV5j0j+wmvmpMxzjN3uvNajLa4zQ8l0Eok9SFaRr2RL0gN8Q2JegfOL4pUiHPsh64WWya2NB7f"
                    + "V+1s65eA5ospXYsShRjo046QhGTmymwXXzdzuxu8IlnTEont6P4+J+GsWk6cldGbl20hctuUKzyx"
                    + "OptjEPOKejV60iDCYGmHbCWAzQ8h5MILV82IclzNViZmzAapeeCnexhpXhWTs+xDEYSKEiG/camt"
                    + "bhmZc3BcyVJrW23PktSfpBQ6D8ZxoMfF0L7V2GQMaUg+3r7ucrx82kpqotjv0xHghNIm95aBr1Qw"
                    + "1gaEjsC/0wGmmBDg1dTDH+F1p9TInzr3EFuYD0YiQ7YlAHq3cPuyGoLXJ5dXYuSBfhDXJSeddUkl"
                    + "k1ufZyOOcskeInQge7jzaRfmKg3U94r+spMEvb0AzDQVOKvjjo1ivxMSgFRZaDb/4qw=";

        private static final String PASSWORD = "android";

        /**
         * Loads a keystore from a base64-encoded String. Returns the KeyManager[]
         * for the result.
         */
        private KeyManager[] getKeyManagers() throws Exception {
            byte[] bytes = Base64.decode(SERVER_KEYS_BKS, Base64.DEFAULT);
            InputStream inputStream = new ByteArrayInputStream(bytes);

            KeyStore keyStore = KeyStore.getInstance(KeyStore.getDefaultType());
            keyStore.load(inputStream, PASSWORD.toCharArray());
            inputStream.close();

            String algorithm = KeyManagerFactory.getDefaultAlgorithm();
            KeyManagerFactory keyManagerFactory = KeyManagerFactory.getInstance(algorithm);
            keyManagerFactory.init(keyStore, PASSWORD.toCharArray());

            return keyManagerFactory.getKeyManagers();
        }

        private void setCurrentRequestSocket(Socket socket) {
            synchronized (mLock) {
                mCurrentRequestSocket = socket;
            }
        }

        private boolean getIsCancelled() {
            synchronized (mLock) {
                return mIsCancelled;
            }
        }

        // Called from non-server thread.
        public void cancelAllRequests() throws IOException {
            synchronized (mLock) {
                mIsCancelled = true;
                if (mCurrentRequestSocket != null) {
                    try {
                        mCurrentRequestSocket.close();
                    } catch (IOException ignored) {
                        // Catching this to ensure the server socket is closed as well.
                    }
                }
            }
            // Any current and subsequent accept call will throw instead of block.
            mSocket.close();
        }

        public ServerThread(int port, boolean ssl) throws Exception {
            super("ServerThread");
            mIsSsl = ssl;
            // If tests are run back-to-back, it may take time for the port to become available.
            // Retry a few times with a sleep to wait for the port.
            int retry = 3;
            while (true) {
                try {
                    if (mIsSsl) {
                        mSslContext = SSLContext.getInstance("TLS");
                        mSslContext.init(getKeyManagers(), null, null);
                        mSocket = mSslContext.getServerSocketFactory().createServerSocket(port);
                    } else {
                        mSocket = new ServerSocket(port);
                    }
                    return;
                } catch (IOException e) {
                    Log.w(TAG, e.getMessage());
                    if (--retry == 0) {
                        throw e;
                    }
                    // sleep in case server socket is still being closed
                    Thread.sleep(1000);
                }
            }
        }

        @Override
        public void run() {
            try {
                while (!getIsCancelled()) {
                    Socket socket = mSocket.accept();
                    try {
                        setCurrentRequestSocket(socket);
                        HTTPRequest request = HTTPRequest.parse(socket.getInputStream());
                        if (request != null) {
                            handleRequest(request, socket.getOutputStream());
                        }
                    } catch (InvalidRequest | IOException e) {
                        Log.e(TAG, e.getMessage());
                    } finally {
                        socket.close();
                    }
                }
            } catch (SocketException e) {
            } catch (IOException e) {
                Log.w(TAG, e.getMessage());
            }
        }
    }
}
