// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.UserDataHost;
import org.chromium.net.NetError;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

/** JNI bridge with content::NavigationHandle */
@JNINamespace("content")
public class NavigationHandle {
    private long mNativeNavigationHandle;
    private boolean mIsInPrimaryMainFrame;
    private boolean mIsRendererInitiated;
    private boolean mIsSameDocument;
    private @PageTransition int mPageTransition;
    private GURL mUrl;
    private GURL mReferrerUrl;
    private GURL mBaseUrlForDataUrl;
    private boolean mHasCommitted;
    private boolean mIsDownload;
    private boolean mIsErrorPage;
    private boolean mIsPrimaryMainFrameFragmentNavigation;
    private boolean mIsValidSearchFormUrl;
    private @NetError int mErrorCode;
    private int mHttpStatusCode;
    private Origin mInitiatorOrigin;
    private boolean mIsPost;
    private boolean mHasUserGesture;
    private boolean mIsRedirect;
    private boolean mIsExternalProtocol;
    private long mNavigationId;
    private boolean mIsPageActivation;
    private boolean mIsReload;
    private UserDataHost mUserDataHost;
    private boolean mIsPdf;
    private String mMimeType;
    private boolean mIsSaveableNavigation;

    public static NavigationHandle createForTesting(
            @NonNull GURL url,
            boolean isRendererInitiated,
            @PageTransition int transition,
            boolean hasUserGesture) {
        return createForTesting(
                url,
                /* isInPrimaryMainFrame= */ true,
                /* isSameDocument= */ false,
                isRendererInitiated,
                transition,
                hasUserGesture,
                /* isReload= */ false,
                /* isSaveableNavigation= */ false);
    }

    public static NavigationHandle createForTesting(
            @NonNull GURL url,
            boolean isInPrimaryMainFrame,
            boolean isSameDocument,
            boolean isRendererInitiated,
            @PageTransition int transition,
            boolean hasUserGesture,
            boolean isReload) {
        return createForTesting(
                url,
                isInPrimaryMainFrame,
                isSameDocument,
                isRendererInitiated,
                transition,
                hasUserGesture,
                isReload,
                /* isSaveableNavigation= */ false);
    }

    public static NavigationHandle createForTesting(
            @NonNull GURL url,
            boolean isInPrimaryMainFrame,
            boolean isSameDocument,
            boolean isRendererInitiated,
            @PageTransition int transition,
            boolean hasUserGesture,
            boolean isReload,
            boolean isSaveableNavigation) {
        NavigationHandle handle = new NavigationHandle(0);
        handle.initialize(
                0,
                url,
                GURL.emptyGURL(),
                GURL.emptyGURL(),
                isInPrimaryMainFrame,
                isSameDocument,
                isRendererInitiated,
                null,
                transition,
                /* isPost= */ false,
                hasUserGesture,
                /* isRedirect= */ false,
                /* isExternalProtocol= */ false,
                /* navigationId= */ 0,
                /* isPageActivation= */ false,
                isReload,
                /* isPdf= */ false,
                /* mimeType= */ "",
                isSaveableNavigation);
        return handle;
    }

    @CalledByNative
    private NavigationHandle(long nativeNavigationHandle) {
        mNativeNavigationHandle = nativeNavigationHandle;
    }

    @CalledByNative
    private void initialize(
            long nativeNavigationHandleProxy,
            @NonNull GURL url,
            @NonNull GURL referrerUrl,
            @NonNull GURL baseUrlForDataUrl,
            boolean isInPrimaryMainFrame,
            boolean isSameDocument,
            boolean isRendererInitiated,
            Origin initiatorOrigin,
            @PageTransition int transition,
            boolean isPost,
            boolean hasUserGesture,
            boolean isRedirect,
            boolean isExternalProtocol,
            long navigationId,
            boolean isPageActivation,
            boolean isReload,
            boolean isPdf,
            String mimeType,
            boolean isSaveableNavigation) {
        mUrl = url;
        mReferrerUrl = referrerUrl;
        mBaseUrlForDataUrl = baseUrlForDataUrl;
        mIsInPrimaryMainFrame = isInPrimaryMainFrame;
        mIsSameDocument = isSameDocument;
        mIsRendererInitiated = isRendererInitiated;
        mInitiatorOrigin = initiatorOrigin;
        mPageTransition = transition;
        mIsPost = isPost;
        mHasUserGesture = hasUserGesture;
        mIsRedirect = isRedirect;
        mIsExternalProtocol = isExternalProtocol;
        mNavigationId = navigationId;
        mIsPageActivation = isPageActivation;
        mIsReload = isReload;
        mIsPdf = isPdf;
        mMimeType = mimeType;
        mIsSaveableNavigation = isSaveableNavigation;
    }

    /**
     * The navigation received a redirect. Called once per redirect.
     *
     * @param url The new URL.
     */
    @CalledByNative
    @VisibleForTesting
    public void didRedirect(GURL url, boolean isExternalProtocol) {
        mUrl = url;
        mIsRedirect = true;
        mIsExternalProtocol = isExternalProtocol;
    }

    /** The navigation finished. Called once per navigation. */
    @CalledByNative
    @VisibleForTesting
    public void didFinish(
            @NonNull GURL url,
            boolean isErrorPage,
            boolean hasCommitted,
            boolean isPrimaryMainFrameFragmentNavigation,
            boolean isDownload,
            boolean isValidSearchFormUrl,
            @PageTransition int transition,
            @NetError int errorCode,
            int httpStatuscode,
            boolean isExternalProtocol,
            boolean isPdf,
            String mimeType,
            boolean isSaveableNavigation) {
        mUrl = url;
        mIsErrorPage = isErrorPage;
        mHasCommitted = hasCommitted;
        mIsPrimaryMainFrameFragmentNavigation = isPrimaryMainFrameFragmentNavigation;
        mIsDownload = isDownload;
        mIsValidSearchFormUrl = isValidSearchFormUrl;
        mPageTransition = transition;
        mErrorCode = errorCode;
        mHttpStatusCode = httpStatuscode;
        mIsExternalProtocol = isExternalProtocol;
        mIsPdf = isPdf;
        mMimeType = mimeType;
        mIsSaveableNavigation = isSaveableNavigation;
    }

    /** Release the C++ pointer. */
    @CalledByNative
    private void release() {
        mNativeNavigationHandle = 0;
    }

    public long nativeNavigationHandlePtr() {
        return mNativeNavigationHandle;
    }

    /**
     * The URL the frame is navigating to.  This may change during the navigation when encountering
     * a server redirect.
     */
    @NonNull
    public GURL getUrl() {
        return mUrl;
    }

    /** The referrer URL for the navigation. */
    @NonNull
    public GURL getReferrerUrl() {
        return mReferrerUrl;
    }

    /** Used for specifying a base URL for pages loaded via data URLs. */
    @NonNull
    public GURL getBaseUrlForDataUrl() {
        return mBaseUrlForDataUrl;
    }

    /**
     * Whether the navigation is taking place in the main frame of the primary
     * frame tree. With MPArch (crbug.com/1164280), a WebContents may have
     * additional frame trees for prerendering pages in addition to the primary
     * frame tree (holding the page currently shown to the user). This remains
     * constant over the navigation lifetime.
     */
    public boolean isInPrimaryMainFrame() {
        return mIsInPrimaryMainFrame;
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
     * didFinishNavigationInPrimaryMainFrame()
     */
    public int httpStatusCode() {
        return mHttpStatusCode;
    }

    /** Returns the page transition type. */
    public @PageTransition int pageTransition() {
        return mPageTransition;
    }

    /** Returns true on same-document navigation with fragment change in the primary main frame. */
    public boolean isPrimaryMainFrameFragmentNavigation() {
        return mIsPrimaryMainFrameFragmentNavigation;
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

    /** Returns true if the navigation is a search. */
    public boolean isValidSearchFormUrl() {
        return mIsValidSearchFormUrl;
    }

    /**
     * Get the Origin that initiated this navigation. May be null in the case of navigations
     * originating from the browser.
     */
    public Origin getInitiatorOrigin() {
        return mInitiatorOrigin;
    }

    /** True if the the navigation method is "POST". */
    public boolean isPost() {
        return mIsPost;
    }

    /** True if the navigation was initiated by the user. */
    public boolean hasUserGesture() {
        return mHasUserGesture;
    }

    /** Is the navigation a redirect (in which case URL is the "target" address). */
    public boolean isRedirect() {
        return mIsRedirect;
    }

    /** True if the target URL can't be handled by Chrome's internal protocol handlers. */
    public boolean isExternalProtocol() {
        return mIsExternalProtocol;
    }

    /** Get a unique ID for this navigation. */
    public long getNavigationId() {
        return mNavigationId;
    }

    /*
     * Whether this navigation is activating an existing page (e.g. served from
     * the BackForwardCache or Prerender).
     */
    public boolean isPageActivation() {
        return mIsPageActivation;
    }

    /** Whether this navigation was initiated by a page reload. */
    public boolean isReload() {
        return mIsReload;
    }

    /** Return any user data which has been set on the NavigationHandle. */
    public UserDataHost getUserDataHost() {
        if (mUserDataHost == null) {
            mUserDataHost = new UserDataHost();
        }
        return mUserDataHost;
    }

    /** Sets the user data host. This should not be considered part of the content API. */
    public void setUserDataHost(UserDataHost userDataHost) {
        mUserDataHost = userDataHost;
    }

    /** Whether the navigation is for PDF content. */
    public boolean isPdf() {
        return mIsPdf;
    }

    /** MIME type of the page. */
    public String getMimeType() {
        return mMimeType;
    }

    /** Whether this navigation can be saved so that it be reloaded or synced. */
    public boolean isSaveableNavigation() {
        return mIsSaveableNavigation;
    }
}
