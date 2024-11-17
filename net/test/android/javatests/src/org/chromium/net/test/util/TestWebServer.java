// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.test.util;

import android.util.Base64;
import android.util.Log;
import android.util.Pair;

import org.chromium.base.ApiCompatibilityUtils;

import java.io.IOException;
import java.io.OutputStream;
import java.io.PrintStream;
import java.nio.charset.Charset;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/**
 * Simple http test server for testing.
 *
 * Extends WebServer with the ability to map requests to prepared responses.
 */
public class TestWebServer extends WebServer {
    private static final String TAG = "TestWebServer";

    private static class Response {
        final byte[] mResponseData;
        final List<Pair<String, String>> mResponseHeaders;
        final boolean mIsRedirect;
        final Runnable mResponseAction;
        final boolean mIsNotFound;
        final boolean mIsNoContent;
        final boolean mForWebSocket;
        final boolean mIsEmptyResponse;

        Response(
                byte[] responseData,
                List<Pair<String, String>> responseHeaders,
                boolean isRedirect,
                boolean isNotFound,
                boolean isNoContent,
                boolean forWebSocket,
                boolean isEmptyResponse,
                Runnable responseAction) {
            mIsRedirect = isRedirect;
            mIsNotFound = isNotFound;
            mIsNoContent = isNoContent;
            mForWebSocket = forWebSocket;
            mIsEmptyResponse = isEmptyResponse;
            mResponseData = responseData;
            mResponseHeaders =
                    responseHeaders == null
                            ? new ArrayList<Pair<String, String>>()
                            : responseHeaders;
            mResponseAction = responseAction;
        }
    }

    // The Maps below are modified on both the client thread and the internal server thread, so
    // need to use a lock when accessing them.
    private final Object mLock = new Object();
    private final Map<String, Response> mResponseMap = new HashMap<String, Response>();
    private final Map<String, Integer> mResponseCountMap = new HashMap<String, Integer>();
    private final Map<String, HTTPRequest> mLastRequestMap = new HashMap<String, HTTPRequest>();

    /**
     * Create and start a local HTTP server instance.
     *
     * @param port Port number the server must use, or 0 to automatically choose a free port.
     * @param ssl True if the server should be using secure sockets.
     * @param additional True if creating an additional server instance.
     */
    private TestWebServer(int port, boolean ssl, boolean additional) throws Exception {
        super(port, ssl, additional);
        setRequestHandler(new Handler());
    }

    private class Handler implements WebServer.RequestHandler {
        @Override
        public void handleRequest(WebServer.HTTPRequest request, OutputStream stream) {
            WebServerPrintStream printStream = new WebServerPrintStream(stream);
            try {
                outputResponse(request, printStream);
            } catch (NoSuchAlgorithmException ignore) {
            } catch (IOException e) {
                Log.w(TAG, e);
            } finally {
                printStream.close();
            }
        }
    }

    /**
     * Create and start a local HTTP server instance. This function must only
     * be called if no other instances were created. You are responsible for
     * calling shutdown() on each instance you create.
     *
     * @param port Port number the server must use, or 0 to automatically choose a free port.
     */
    public static TestWebServer start(int port) throws Exception {
        return new TestWebServer(port, false, false);
    }

    /** Same as start(int) but chooses a free port. */
    public static TestWebServer start() throws Exception {
        return start(0);
    }

    /**
     * Create and start a local HTTP server instance. This function must only
     * be called if you need more than one server instance and the first one
     * was already created using start() or start(int). You are responsible for
     * calling shutdown() on each instance you create.
     *
     * @param port Port number the server must use, or 0 to automatically choose a free port.
     */
    public static TestWebServer startAdditional(int port) throws Exception {
        return new TestWebServer(port, false, true);
    }

    /** Same as startAdditional(int) but chooses a free port. */
    public static TestWebServer startAdditional() throws Exception {
        return startAdditional(0);
    }

    /**
     * Create and start a local secure HTTP server instance. This function must
     * only be called if no other secure instances were created. You are
     * responsible for calling shutdown() on each instance you create.
     *
     * @param port Port number the server must use, or 0 to automatically choose a free port.
     */
    public static TestWebServer startSsl(int port) throws Exception {
        return new TestWebServer(port, true, false);
    }

    /** Same as startSsl(int) but chooses a free port. */
    public static TestWebServer startSsl() throws Exception {
        return startSsl(0);
    }

    /**
     * Create and start a local secure HTTP server instance. This function must
     * only be called if you need more than one secure server instance and the
     * first one was already created using startSsl() or startSsl(int). You are
     * responsible for calling shutdown() on each instance you create.
     *
     * @param port Port number the server must use, or 0 to automatically choose a free port.
     */
    public static TestWebServer startAdditionalSsl(int port) throws Exception {
        return new TestWebServer(port, true, true);
    }

    /** Same as startAdditionalSsl(int) but chooses a free port. */
    public static TestWebServer startAdditionalSsl() throws Exception {
        return startAdditionalSsl(0);
    }

    private static final int RESPONSE_STATUS_NORMAL = 0;
    private static final int RESPONSE_STATUS_MOVED_TEMPORARILY = 1;
    private static final int RESPONSE_STATUS_NOT_FOUND = 2;
    private static final int RESPONSE_STATUS_NO_CONTENT = 3;
    private static final int RESPONSE_STATUS_FOR_WEBSOCKET = 4;
    private static final int RESPONSE_STATUS_EMPTY_RESPONSE = 5;

    private String setResponseInternal(
            String requestPath,
            byte[] responseData,
            List<Pair<String, String>> responseHeaders,
            Runnable responseAction,
            int status) {
        final boolean isRedirect = (status == RESPONSE_STATUS_MOVED_TEMPORARILY);
        final boolean isNotFound = (status == RESPONSE_STATUS_NOT_FOUND);
        final boolean isNoContent = (status == RESPONSE_STATUS_NO_CONTENT);
        final boolean forWebSocket = (status == RESPONSE_STATUS_FOR_WEBSOCKET);
        final boolean isEmptyResponse = (status == RESPONSE_STATUS_EMPTY_RESPONSE);

        synchronized (mLock) {
            mResponseMap.put(
                    requestPath,
                    new Response(
                            responseData,
                            responseHeaders,
                            isRedirect,
                            isNotFound,
                            isNoContent,
                            forWebSocket,
                            isEmptyResponse,
                            responseAction));
            mResponseCountMap.put(requestPath, Integer.valueOf(0));
            mLastRequestMap.put(requestPath, null);
        }
        return getResponseUrl(requestPath);
    }

    /**
     * Sets a 404 (not found) response to be returned when a particular request path is passed in.
     *
     * @param requestPath The path to respond to.
     * @return The full URL including the path that should be requested to get the expected
     *         response.
     */
    public String setResponseWithNotFoundStatus(String requestPath) {
        return setResponseWithNotFoundStatus(requestPath, null);
    }

    /**
     * Sets a 404 (not found) response to be returned when a particular request path is passed in.
     *
     * @param requestPath The path to respond to.
     * @param responseHeaders Any additional headers that should be returned along with the
     *                        response (null is acceptable).
     * @return The full URL including the path that should be requested to get the expected
     *         response.
     */
    public String setResponseWithNotFoundStatus(
            String requestPath, List<Pair<String, String>> responseHeaders) {
        return setResponseInternal(
                requestPath,
                ApiCompatibilityUtils.getBytesUtf8(""),
                responseHeaders,
                null,
                RESPONSE_STATUS_NOT_FOUND);
    }

    /**
     * Sets a 204 (no content) response to be returned when a particular request path is passed in.
     *
     * @param requestPath The path to respond to.
     * @return The full URL including the path that should be requested to get the expected
     *         response.
     */
    public String setResponseWithNoContentStatus(String requestPath) {
        return setResponseInternal(
                requestPath,
                ApiCompatibilityUtils.getBytesUtf8(""),
                null,
                null,
                RESPONSE_STATUS_NO_CONTENT);
    }

    /**
     * Sets an empty response to be returned when a particular request path is passed in.
     *
     * @param requestPath The path to respond to.
     * @return The full URL including the path that should be requested to get the expected
     *         response.
     */
    public String setEmptyResponse(String requestPath) {
        return setResponseInternal(
                requestPath,
                ApiCompatibilityUtils.getBytesUtf8(""),
                null,
                null,
                RESPONSE_STATUS_EMPTY_RESPONSE);
    }

    /**
     * Sets a response to be returned when a particular request path is passed
     * in (with the option to specify additional headers).
     *
     * @param requestPath The path to respond to.
     * @param responseString The response body that will be returned.
     * @param responseHeaders Any additional headers that should be returned along with the
     *                        response (null is acceptable).
     * @return The full URL including the path that should be requested to get the expected
     *         response.
     */
    public String setResponse(
            String requestPath, String responseString, List<Pair<String, String>> responseHeaders) {
        return setResponseInternal(
                requestPath,
                ApiCompatibilityUtils.getBytesUtf8(responseString),
                responseHeaders,
                null,
                RESPONSE_STATUS_NORMAL);
    }

    /**
     * Sets a response to be returned when a particular request path is passed
     * in with the option to specify additional headers as well as an arbitrary action to be
     * executed on each request.
     *
     * @param requestPath The path to respond to.
     * @param responseString The response body that will be returned.
     * @param responseHeaders Any additional headers that should be returned along with the
     *                        response (null is acceptable).
     * @param responseAction The action to be performed when fetching the response.  This action
     *                       will be executed for each request and will be handled on a background
     *                       thread.
     * @return The full URL including the path that should be requested to get the expected
     *         response.
     */
    public String setResponseWithRunnableAction(
            String requestPath,
            String responseString,
            List<Pair<String, String>> responseHeaders,
            Runnable responseAction) {
        return setResponseInternal(
                requestPath,
                ApiCompatibilityUtils.getBytesUtf8(responseString),
                responseHeaders,
                responseAction,
                RESPONSE_STATUS_NORMAL);
    }

    /**
     * Sets a redirect.
     *
     * @param requestPath The path to respond to.
     * @param targetLocation The path (or absolute URL) to redirect to.
     * @return The full URL including the path that should be requested to get the expected
     *         response.
     */
    public String setRedirect(String requestPath, String targetLocation) {
        return setRedirect(requestPath, targetLocation, new ArrayList<>());
    }

    /**
     * Sets a redirect with optional headers.
     *
     * @param requestPath The path to respond to.
     * @param targetLocation The path (or absolute URL) to redirect to.
     * @param responseHeaders Any additional headers that should be returned along with the
     *                        response (null is acceptable).
     * @return The full URL including the path that should be requested to get the expected
     *         response.
     */
    public String setRedirect(
            String requestPath, String targetLocation, List<Pair<String, String>> responseHeaders) {
        responseHeaders.add(Pair.create("Location", targetLocation));

        return setResponseInternal(
                requestPath,
                ApiCompatibilityUtils.getBytesUtf8(targetLocation),
                responseHeaders,
                null,
                RESPONSE_STATUS_MOVED_TEMPORARILY);
    }

    /**
     * Sets a base64 encoded response to be returned when a particular request path is passed
     * in (with the option to specify additional headers).
     *
     * @param requestPath The path to respond to.
     * @param base64EncodedResponse The response body that is base64 encoded. The actual server
     *                              response will the decoded binary form.
     * @param responseHeaders Any additional headers that should be returned along with the
     *                        response (null is acceptable).
     * @return The full URL including the path that should be requested to get the expected
     *         response.
     */
    public String setResponseBase64(
            String requestPath,
            String base64EncodedResponse,
            List<Pair<String, String>> responseHeaders) {
        return setResponseInternal(
                requestPath,
                Base64.decode(base64EncodedResponse, Base64.DEFAULT),
                responseHeaders,
                null,
                RESPONSE_STATUS_NORMAL);
    }

    /**
     * Sets a response to a WebSocket handshake request.
     *
     * @param requestPath The path to respond to.
     * @param responseHeaders Any additional headers that should be returned along with the
     *                        response (null is acceptable).
     * @return The full URL including the path that should be requested to get the expected
     *         response.
     */
    public String setResponseForWebSocket(
            String requestPath, List<Pair<String, String>> responseHeaders) {
        if (responseHeaders == null) {
            responseHeaders = new ArrayList<Pair<String, String>>();
        } else {
            responseHeaders = new ArrayList<Pair<String, String>>(responseHeaders);
        }
        responseHeaders.add(Pair.create("Connection", "Upgrade"));
        responseHeaders.add(Pair.create("Upgrade", "websocket"));
        return setResponseInternal(
                requestPath,
                ApiCompatibilityUtils.getBytesUtf8(""),
                responseHeaders,
                null,
                RESPONSE_STATUS_FOR_WEBSOCKET);
    }

    /** Get the number of requests was made at this path since it was last set. */
    public int getRequestCount(String requestPath) {
        Integer count = null;
        synchronized (mLock) {
            count = mResponseCountMap.get(requestPath);
        }
        if (count == null) throw new IllegalArgumentException("Path not set: " + requestPath);
        return count.intValue();
    }

    /** Returns the last HttpRequest at this path. Can return null if it is never requested. */
    public HTTPRequest getLastRequest(String requestPath) {
        synchronized (mLock) {
            if (!mLastRequestMap.containsKey(requestPath)) {
                throw new IllegalArgumentException("Path not set: " + requestPath);
            }
            return mLastRequestMap.get(requestPath);
        }
    }

    private static class WebServerPrintStream extends PrintStream {
        WebServerPrintStream(OutputStream out) {
            super(out);
        }

        @Override
        public void println(String s) {
            Log.w(TAG, s);
            super.println(s);
        }
    }

    /**
     * Generate a response to the given request.
     *
     * <p>Always executed on the background server thread.
     *
     * <p>If there is an action associated with the response, it will be executed inside of
     * this function.
     *
     * @throws NoSuchAlgorithmException, IOException
     */
    private void outputResponse(HTTPRequest request, WebServerPrintStream stream)
            throws NoSuchAlgorithmException, IOException {
        // Don't dump headers to decrease log.
        Log.w(TAG, request.requestLine());

        final String bodyTemplate =
                "<html><head><title>%s</title></head>" + "<body>%s</body></html>";

        boolean copyHeadersToResponse = true;
        boolean copyBinaryBodyToResponse = false;
        boolean contentLengthAlreadyIncluded = false;
        boolean contentTypeAlreadyIncluded = false;
        StringBuilder textBody = new StringBuilder();

        String requestURI = request.getURI();

        Response response;
        synchronized (mLock) {
            response = mResponseMap.get(requestURI);
        }

        if (response == null || response.mIsNotFound) {
            stream.println("HTTP/1.0 404 Not Found");
            textBody.append(String.format(bodyTemplate, "Not Found", "Not Found"));
        } else if (response.mForWebSocket) {
            String keyHeader = request.headerValue("Sec-WebSocket-Key");
            if (!keyHeader.isEmpty()) {
                stream.println("HTTP/1.0 101 Switching Protocols");
                stream.println("Sec-WebSocket-Accept: " + computeWebSocketAccept(keyHeader));
            } else {
                stream.println("HTTP/1.0 404 Not Found");
                textBody.append(String.format(bodyTemplate, "Not Found", "Not Found"));
                copyHeadersToResponse = false;
            }
        } else if (response.mIsNoContent) {
            stream.println("HTTP/1.0 204 No Content");
            copyHeadersToResponse = false;
        } else if (response.mIsRedirect) {
            stream.println("HTTP/1.0 302 Found");
            textBody.append(String.format(bodyTemplate, "Found", "Found"));
        } else if (response.mIsEmptyResponse) {
            stream.println("HTTP/1.0 200 OK");
            copyHeadersToResponse = false;
        } else {
            if (response.mResponseAction != null) response.mResponseAction.run();

            stream.println("HTTP/1.0 200 OK");
            copyBinaryBodyToResponse = true;
        }

        if (response != null) {
            if (copyHeadersToResponse) {
                for (Pair<String, String> header : response.mResponseHeaders) {
                    stream.println(header.first + ": " + header.second);
                    if (header.first.toLowerCase(Locale.ENGLISH).equals("content-length")) {
                        contentLengthAlreadyIncluded = true;
                    } else if (header.first.toLowerCase(Locale.ENGLISH).equals("content-type")) {
                        contentTypeAlreadyIncluded = true;
                    }
                }
            }
            synchronized (mLock) {
                mResponseCountMap.put(
                        requestURI,
                        Integer.valueOf(mResponseCountMap.get(requestURI).intValue() + 1));
                mLastRequestMap.put(requestURI, request);
            }
        }

        // RFC 1123
        final SimpleDateFormat dateFormat =
                new SimpleDateFormat("EEE, dd MMM yyyy HH:mm:ss z", Locale.US);

        // Using print and println() because we don't want to dump it into log.
        stream.print("Date: " + dateFormat.format(new Date()));
        stream.println();

        if (textBody.length() != 0) {
            if (!contentTypeAlreadyIncluded
                    && (requestURI.endsWith(".html") || requestURI.endsWith(".htm"))) {
                stream.println("Content-Type: text/html");
            }
            stream.println("Content-Length: " + textBody.length());
            stream.println();
            stream.print(textBody.toString());
        } else if (copyBinaryBodyToResponse) {
            if (!contentTypeAlreadyIncluded && requestURI.endsWith(".js")) {
                stream.println("Content-Type: application/javascript");
            } else if (!contentTypeAlreadyIncluded
                    && (requestURI.endsWith(".html") || requestURI.endsWith(".htm"))) {
                stream.println("Content-Type: text/html");
            }
            if (!contentLengthAlreadyIncluded) {
                stream.println("Content-Length: " + response.mResponseData.length);
            }
            stream.println();
            stream.write(response.mResponseData);
        } else {
            stream.println();
        }
    }

    /** Return a response for WebSocket handshake challenge. */
    private static String computeWebSocketAccept(String keyString) throws NoSuchAlgorithmException {
        byte[] key = keyString.getBytes(Charset.forName("US-ASCII"));
        byte[] guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11".getBytes(Charset.forName("US-ASCII"));

        MessageDigest md = MessageDigest.getInstance("SHA");
        md.update(key);
        md.update(guid);
        byte[] output = md.digest();
        return Base64.encodeToString(output, Base64.NO_WRAP);
    }
}
