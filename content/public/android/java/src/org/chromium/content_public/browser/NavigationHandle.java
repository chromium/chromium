// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.net.NetError;

/**
 * JNI bridge with content::NavigationHandle
 */
@JNINamespace("content")
public class NavigationHandle {
    private long mNativeNavigationHandleProxy;
    private final boolean mIsInMainFrame;
    private final boolean mIsRendererInitiated;
    private final boolean mIsSameDocument;
    private Integer mPageTransition;
    private String mUrl;
    private boolean mHasCommitted;
    private boolean mIsDownload;
    private boolean mIsErrorPage;
    private boolean mIsFragmentNavigation;
    private boolean mIsValidSearchFormUrl;
    private @NetError int mErrorCode;
    private int mHttpStatusCode;

    @CalledByNative
    public NavigationHandle(long nativeNavigationHandleProxy, String url, boolean isInMainFrame,
            boolean isSameDocument, boolean isRendererInitiated) {
        mNativeNavigationHandleProxy = nativeNavigationHandleProxy;
        mUrl = url;
        mIsInMainFrame = isInMainFrame;
        mIsSameDocument = isSameDocument;
        mIsRendererInitiated = isRendererInitiated;
    }

    /**
     * The navigation received a redirect. Called once per redirect.
     * @param url The new URL.
     */
    @CalledByNative
    private void didRedirect(String url) {
        mUrl = url;
    }

    /**
     * The navigation finished. Called once per navigation.
     */
    @CalledByNative
    public void didFinish(String url, boolean isErrorPage, boolean hasCommitted,
            boolean isFragmentNavigation, boolean isDownload, boolean isValidSearchFormUrl,
            int transition, @NetError int errorCode, int httpStatuscode) {
        mUrl = url;
        mIsErrorPage = isErrorPage;
        mHasCommitted = hasCommitted;
        mIsFragmentNavigation = isFragmentNavigation;
        mIsDownload = isDownload;
        mIsValidSearchFormUrl = isValidSearchFormUrl;
        mPageTransition = transition == -1 ? null : transition;
        mErrorCode = errorCode;
        mHttpStatusCode = httpStatuscode;
    }

    /**
     * Release the C++ pointer.
     */
    @CalledByNative
    private void release() {
        mNativeNavigationHandleProxy = 0;
    }

    public long nativePtr() {
        return mNativeNavigationHandleProxy;
    }

    /**
     * The URL the frame is navigating to.  This may change during the navigation when encountering
     * a server redirect.
     */
    public String getUrl() {
        return mUrl;
    }

    /**
     * Whether the navigation is taking place in the main frame or in a subframe.
     */
    public boolean isInMainFrame() {
        return mIsInMainFrame;
    }

    /**
     * Whether the navigation was initiated by the renderer process. Examples of renderer-initiated
     * navigations include:
     *  - <a> link click
     *  - changing window.location.href
     *  - redirect via the <meta http-equiv="refresh"> tag
     *  - using window.history.pushState
     *
     * This method returns false for browser-initiated navigations, including:
     *  - any navigation initiated from the omnibox
     *  - navigations via suggestions in browser UI
     *  - navigations via browser UI: Ctrl-R, refresh/forward/back/home buttons
     *  - using window.history.forward() or window.history.back()
     *  - any other "explicit" URL navigations, e.g. bookmarks
     */
    public boolean isRendererInitiated() {
        return mIsRendererInitiated;
    }

    /**
     * Whether the navigation happened without changing document.
     * Examples of same document navigations are:
     * - reference fragment navigations
     * - pushState/replaceState
     * - same page history navigation
     */
    public boolean isSameDocument() {
        return mIsSameDocument;
    }

    public String errorDescription() {
        // TODO(shaktisahu): Provide appropriate error description (crbug/690784).
        return "";
    }

    public @NetError int errorCode() {
        return mErrorCode;
    }

    /**
     * Whether the navigation has committed. Navigations that end up being downloads or return
     * 204/205 response codes do not commit (i.e. the WebContents stays at the existing URL). This
     * returns true for either successful commits or error pages that replace the previous page
     * (distinguished by |IsErrorPage|), and false for errors that leave the user on the previous
     * page.
     */
    public boolean hasCommitted() {
        return mHasCommitted;
    }

    /**
     * Return the HTTP status code. This can be used after the response is received in
     * didFinishNavigation()
     */
    public int httpStatusCode() {
        return mHttpStatusCode;
    }

    /**
     * Returns the page transition type.
     */
    public Integer pageTransition() {
        return mPageTransition;
    }

    /**
     * Returns true on same-document navigation with fragment change.
     */
    public boolean isFragmentNavigation() {
        return mIsFragmentNavigation;
    }

    /**
     * Whether the navigation resulted in an error page.
     * Note that if an error page reloads, this will return true even though GetNetErrorCode will be
     * net::OK.
     */
    public boolean isErrorPage() {
        return mIsErrorPage;
    }

    /**
     * Returns true if this navigation resulted in a download. Returns false if this navigation did
     * not result in a download, or if download status is not yet known for this navigation.
     * Download status is determined for a navigation when processing final (post redirect) HTTP
     * response headers.
     */
    public boolean isDownload() {
        return mIsDownload;
    }

    /**
     * Returns true if the navigation is a search.
     */
    public boolean isValidSearchFormUrl() {
        return mIsValidSearchFormUrl;
    }

    /**
     * Set request's header. If the header is already present, its value is overwritten. When
     * modified during a navigation start, the headers will be applied to the initial network
     * request. When modified during a redirect, the headers will be applied to the redirected
     * request.
     */
    public void setRequestHeader(String headerName, String headerValue) {
        NavigationHandleJni.get().setRequestHeader(
                mNativeNavigationHandleProxy, headerName, headerValue);
    }

    /**
     * Remove a request's header. If the header is not present, it has no effect. Must be called
     * during a redirect.
     */
    public void removeRequestHeader(String headerName) {
        NavigationHandleJni.get().removeRequestHeader(mNativeNavigationHandleProxy, headerName);
    }

    @NativeMethods
    interface Natives {
        void setRequestHeader(
                long nativeNavigationHandleProxy, String headerName, String headerValue);
        void removeRequestHeader(long nativeNavigationHandleProxy, String headerName);
    }
}
