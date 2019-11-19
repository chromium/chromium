// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.navigation_controller.LoadURLType;
import org.chromium.content_public.browser.navigation_controller.UserAgentOverrideOption;
import org.chromium.content_public.common.Referrer;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.ui.base.PageTransition;

import java.util.Locale;
import java.util.Map;

/**
 * Holds parameters for NavigationController.LoadUrl. Parameters should match
 * counterparts in NavigationController::LoadURLParams, including default
 * values.
 */
@JNINamespace("content")
public class LoadUrlParams {
    // Fields with counterparts in NavigationController::LoadURLParams.
    // Package private so that NavigationController.loadUrl can pass them down to
    // native code. Should not be accessed directly anywhere else outside of
    // this class.
    String mUrl;
    // TODO(nasko,tedchoc): https://crbug.com/980641: Don't use String to store
    // initiator origin, as it is a lossy format.
    String mInitiatorOrigin;
    int mLoadUrlType;
    int mTransitionType;
    Referrer mReferrer;
    private Map<String, String> mExtraHeaders;
    private String mVerbatimHeaders;
    int mUaOverrideOption;
    ResourceRequestBody mPostData;
    String mBaseUrlForDataUrl;
    String mVirtualUrlForDataUrl;
    String mDataUrlAsString;
    boolean mCanLoadLocalResources;
    boolean mIsRendererInitiated;
    boolean mShouldReplaceCurrentEntry;
    long mIntentReceivedTimestamp;
    long mInputStartTimestamp;
    boolean mHasUserGesture;
    boolean mShouldClearHistoryList;

    /**
     * Creates an instance with default page transition type.
     * @param url the url to be loaded
     */
    public LoadUrlParams(String url) {
        this(url, PageTransition.LINK);
    }

    /**
     * Creates an instance with the given page transition type.
     * @param url the url to be loaded
     * @param transitionType the PageTransitionType constant corresponding to the load
     */
    public LoadUrlParams(String url, int transitionType) {
        mUrl = url;
        mTransitionType = transitionType;

        // Initialize other fields to defaults matching defaults of the native
        // NavigationController::LoadUrlParams.
        mLoadUrlType = LoadURLType.DEFAULT;
        mUaOverrideOption = UserAgentOverrideOption.INHERIT;
        mPostData = null;
        mBaseUrlForDataUrl = null;
        mVirtualUrlForDataUrl = null;
        mDataUrlAsString = null;
    }

    /**
     * Helper method to create a LoadUrlParams object for data url.
     * @param data Data to be loaded.
     * @param mimeType Mime type of the data.
     * @param isBase64Encoded True if the data is encoded in Base 64 format.
     */
    public static LoadUrlParams createLoadDataParams(
            String data, String mimeType, boolean isBase64Encoded) {
        return createLoadDataParams(data, mimeType, isBase64Encoded, null);
    }

    /**
     * Helper method to create a LoadUrlParams object for data url.
     * @param data Data to be loaded.
     * @param mimeType Mime type of the data.
     * @param isBase64Encoded True if the data is encoded in Base 64 format.
     * @param charset The character set for the data. Pass null if the mime type
     *                does not require a special charset.
     */
    public static LoadUrlParams createLoadDataParams(
            String data, String mimeType, boolean isBase64Encoded, String charset) {
        LoadUrlParams params = new LoadUrlParams(
                buildDataUri(data, mimeType, isBase64Encoded, charset));
        params.setLoadType(LoadURLType.DATA);
        params.setTransitionType(PageTransition.TYPED);
        return params;
    }

    private static String buildDataUri(
            String data, String mimeType, boolean isBase64Encoded, String charset) {
        StringBuilder dataUrl = new StringBuilder("data:");
        dataUrl.append(mimeType);
        if (charset != null && !charset.isEmpty()) {
            dataUrl.append(";charset=" + charset);
        }
        if (isBase64Encoded) {
            dataUrl.append(";base64");
        }
        dataUrl.append(",");
        dataUrl.append(data);
        return dataUrl.toString();
    }

    /**
     * Helper method to create a LoadUrlParams object for data url with base
     * and virtual url.
     * @param data Data to be loaded.
     * @param mimeType Mime type of the data.
     * @param isBase64Encoded True if the data is encoded in Base 64 format.
     * @param baseUrl Base url of this data load. Note that for WebView compatibility,
     *                baseUrl and historyUrl are ignored if this is a data: url.
     *                Defaults to about:blank if null.
     * @param historyUrl History url for this data load. Note that for WebView compatibility,
     *                   this is ignored if baseUrl is a data: url. Defaults to about:blank
     *                   if null.
     */
    public static LoadUrlParams createLoadDataParamsWithBaseUrl(
            String data, String mimeType, boolean isBase64Encoded,
            String baseUrl, String historyUrl) {
        return createLoadDataParamsWithBaseUrl(data, mimeType, isBase64Encoded,
                baseUrl, historyUrl, null);
    }

    /**
     * Helper method to create a LoadUrlParams object for data url with base
     * and virtual url.
     * @param data Data to be loaded.
     * @param mimeType Mime type of the data.
     * @param isBase64Encoded True if the data is encoded in Base 64 format.
     * @param baseUrl Base url of this data load. Note that for WebView compatibility,
     *                baseUrl and historyUrl are ignored if this is a data: url.
     *                Defaults to about:blank if null.
     * @param historyUrl History url for this data load. Note that for WebView compatibility,
     *                   this is ignored if baseUrl is a data: url. Defaults to about:blank
     *                   if null.
     * @param charset The character set for the data. Pass null if the mime type
     *                does not require a special charset.
     */
    public static LoadUrlParams createLoadDataParamsWithBaseUrl(
            String data, String mimeType, boolean isBase64Encoded,
            String baseUrl, String historyUrl, String charset) {
        LoadUrlParams params;
        // For WebView compatibility, when the base URL has the 'data:'
        // scheme, we treat it as a regular data URL load and skip setting
        // baseUrl and historyUrl.
        // TODO(joth): we should just append baseURL and historyURL here, and move the
        // WebView specific transform up to a wrapper factory function in android_webview/.
        if (baseUrl == null || !baseUrl.toLowerCase(Locale.US).startsWith("data:")) {
            params = createLoadDataParams("", mimeType, isBase64Encoded, charset);
            params.setBaseUrlForDataUrl(baseUrl != null ? baseUrl : "about:blank");
            params.setVirtualUrlForDataUrl(historyUrl != null ? historyUrl : "about:blank");
            params.setDataUrlAsString(buildDataUri(data, mimeType, isBase64Encoded, charset));
        } else {
            params = createLoadDataParams(data, mimeType, isBase64Encoded, charset);
        }
        return params;
    }

    /**
     * Helper method to create a LoadUrlParams object for an HTTP POST load.
     * @param url URL of the load.
     * @param postData Post data of the load. Can be null.
     */
    @VisibleForTesting
    public static LoadUrlParams createLoadHttpPostParams(
            String url, byte[] postData) {
        LoadUrlParams params = new LoadUrlParams(url);
        params.setLoadType(LoadURLType.HTTP_POST);
        params.setTransitionType(PageTransition.TYPED);
        params.setPostData(ResourceRequestBody.createFromBytes(postData));
        return params;
    }

    /**
     * Sets the url.
     */
    public void setUrl(String url) {
        mUrl = url;
    }

    /**
     * Return the url.
     */
    public String getUrl() {
        return mUrl;
    }

    /**
     * Sets the initiator origin.
     */
    public void setInitiatorOrigin(String initiatorOrigin) {
        mInitiatorOrigin = initiatorOrigin;
    }

    /**
     * Return the initiator origin.
     */
    public @Nullable String getInitiatorOrigin() {
        return mInitiatorOrigin;
    }

    /**
     * Return the base url for a data url, otherwise null.
     */
    public String getBaseUrl() {
        return mBaseUrlForDataUrl;
    }

    /**
     * Set load type of this load. Defaults to LoadURLType.DEFAULT.
     * @param loadType One of LOAD_TYPE static constants above.
     */
    public void setLoadType(int loadType) {
        mLoadUrlType = loadType;
    }

    /**
     * Set transition type of this load. Defaults to PageTransition.LINK.
     * @param transitionType One of PAGE_TRANSITION static constants in ContentView.
     */
    public void setTransitionType(int transitionType) {
        mTransitionType = transitionType;
    }

    /**
     * Return the transition type.
     */
    public int getTransitionType() {
        return mTransitionType;
    }

    /**
     * Sets the referrer of this load.
     */
    public void setReferrer(Referrer referrer) {
        mReferrer = referrer;
    }

    /**
     * @return the referrer of this load.
     */
    public Referrer getReferrer() {
        return mReferrer;
    }

    /**
     * Set extra headers for this load.
     * @param extraHeaders Extra HTTP headers for this load. Note that these
     *                     headers will never overwrite existing ones set by Chromium.
     */
    public void setExtraHeaders(Map<String, String> extraHeaders) {
        mExtraHeaders = extraHeaders;
    }

    /**
     * Return the extra headers as a map.
     */
    public Map<String, String> getExtraHeaders() {
        return mExtraHeaders;
    }

    /**
     * Return the extra headers as a single String separated by "\n", or null if no extra header is
     * set. This form is suitable for passing to native
     * NavigationController::LoadUrlParams::extra_headers. This will return the headers set in an
     * exploded form through setExtraHeaders(). Embedders that work with extra headers in opaque
     * collapsed form can use the setVerbatimHeaders() / getVerbatimHeaders() instead.
     */
    public String getExtraHeadersString() {
        return getExtraHeadersString("\n", false);
    }

    /**
     * Return the extra headers as a single String separated by "\r\n", or null if no extra header
     * is set. This form is suitable for passing to native
     * net::HttpRequestHeaders::AddHeadersFromString.
     */
    public String getExtraHttpRequestHeadersString() {
        return getExtraHeadersString("\r\n", true);
    }

    private String getExtraHeadersString(String delimiter, boolean addTerminator) {
        if (mExtraHeaders == null) return null;

        StringBuilder headerBuilder = new StringBuilder();
        for (Map.Entry<String, String> header : mExtraHeaders.entrySet()) {
            if (headerBuilder.length() > 0) headerBuilder.append(delimiter);

            // Header name should be lower case.
            headerBuilder.append(header.getKey().toLowerCase(Locale.US));
            headerBuilder.append(":");
            headerBuilder.append(header.getValue());
        }
        if (addTerminator) headerBuilder.append(delimiter);

        return headerBuilder.toString();
    }

    /**
     * Sets the verbatim extra headers string. This is an alternative to storing the headers in
     * a map (setExtraHeaders()) for the embedders that use collapsed headers strings.
     */
    public void setVerbatimHeaders(String headers) {
        mVerbatimHeaders = headers;
    }

    /**
     * @return the verbatim extra headers string
     */
    public String getVerbatimHeaders() {
        return mVerbatimHeaders;
    }

    /**
     * Set user agent override option of this load. Defaults to UserAgentOverrideOption.INHERIT.
     * @param uaOption One of UA_OVERRIDE static constants above.
     */
    public void setOverrideUserAgent(int uaOption) {
        mUaOverrideOption = uaOption;
    }

    /**
     * Get user agent override option of this load. Defaults to UserAgentOverrideOption.INHERIT.
     * @param uaOption One of UA_OVERRIDE static constants above.
     */
    public int getUserAgentOverrideOption() {
        return mUaOverrideOption;
    }

    /**
     * Set the post data of this load. This field is ignored unless load type is
     * LoadURLType.HTTP_POST.
     * @param postData Post data for this http post load.
     */
    public void setPostData(ResourceRequestBody postData) {
        mPostData = postData;
    }

    /**
     * @return the data to be sent through POST
     */
    public ResourceRequestBody getPostData() {
        return mPostData;
    }

    /**
     * Set the base url for data load. It is used both to resolve relative URLs
     * and when applying JavaScript's same origin policy. It is ignored unless
     * load type is LoadURLType.DATA.
     * @param baseUrl The base url for this data load.
     */
    public void setBaseUrlForDataUrl(String baseUrl) {
        mBaseUrlForDataUrl = baseUrl;
    }

    /**
     * Get the virtual url for data load. It is the url displayed to the user.
     * It is ignored unless load type is LoadURLType.DATA.
     * @return The virtual url for this data load.
     */
    public String getVirtualUrlForDataUrl() {
        return mVirtualUrlForDataUrl;
    }

    /**
     * Set the virtual url for data load. It is the url displayed to the user.
     * It is ignored unless load type is LoadURLType.DATA.
     * @param virtualUrl The virtual url for this data load.
     */
    public void setVirtualUrlForDataUrl(String virtualUrl) {
        mVirtualUrlForDataUrl = virtualUrl;
    }

    /**
     * Get the data for data load. This is then passed to the renderer as
     * a string, not as a GURL object to circumvent GURL size restriction.
     * @return The data url.
     */
    public String getDataUrlAsString() {
        return mDataUrlAsString;
    }

    /**
     * Set the data for data load. This is then passed to the renderer as
     * a string, not as a GURL object to circumvent GURL size restriction.
     * @param url The data url.
     */
    public void setDataUrlAsString(String url) {
        mDataUrlAsString = url;
    }

    /**
     * Set whether the load should be able to access local resources. This
     * defaults to false.
     */
    public void setCanLoadLocalResources(boolean canLoad) {
        mCanLoadLocalResources = canLoad;
    }

    /**
     * Get whether the load should be able to access local resources. This
     * defaults to false.
     */
    public boolean getCanLoadLocalResources() {
        return mCanLoadLocalResources;
    }

    public int getLoadUrlType() {
        return mLoadUrlType;
    }

    /**
     * @param rendererInitiated Whether or not this load was initiated from a renderer.
     */
    public void setIsRendererInitiated(boolean rendererInitiated) {
        mIsRendererInitiated = rendererInitiated;
    }

    /**
     * @return Whether or not this load was initiated from a renderer or not.
     */
    public boolean getIsRendererInitiated() {
        return mIsRendererInitiated;
    }

    /**
     * @param shouldReplaceCurrentEntry Whether this navigation should replace
     * the current navigation entry.
     */
    public void setShouldReplaceCurrentEntry(boolean shouldReplaceCurrentEntry) {
        mShouldReplaceCurrentEntry = shouldReplaceCurrentEntry;
    }

    /**
     * @return Whether this navigation should replace the current navigation
     * entry.
     */
    public boolean getShouldReplaceCurrentEntry() {
        return mShouldReplaceCurrentEntry;
    }

    /**
     * @param intentReceivedTimestamp the timestamp at which Chrome received the intent that
     *                                triggered this URL load, as returned by System.currentMillis.
     */
    public void setIntentReceivedTimestamp(long intentReceivedTimestamp) {
        mIntentReceivedTimestamp = intentReceivedTimestamp;
    }

    /**
     * @return The timestamp at which Chrome received the intent that triggered this URL load.
     */
    public long getIntentReceivedTimestamp() {
        return mIntentReceivedTimestamp;
    }

    /**
     * @param inputStartTimestamp the timestamp of the event in the location bar that triggered
     *                            this URL load, as returned by System.currentMillis.
     */
    public void setInputStartTimestamp(long inputStartTimestamp) {
        mInputStartTimestamp = inputStartTimestamp;
    }

    /**
     * @return The timestamp of the event in the location bar that triggered this URL load.
     */
    public long getInputStartTimestamp() {
        return mInputStartTimestamp;
    }

    /**
     * Set whether the load is initiated by a user gesture.
     *
     * @param hasUserGesture True if load is initiated by user gesture, or false otherwise.
     */
    public void setHasUserGesture(boolean hasUserGesture) {
        mHasUserGesture = hasUserGesture;
    }

    /**
     * @return Whether or not this load was initiated with a user gesture.
     */
    public boolean getHasUserGesture() {
        return mHasUserGesture;
    }

    /** Sets whether session history should be cleared once the navigation commits. */
    public void setShouldClearHistoryList(boolean shouldClearHistoryList) {
        mShouldClearHistoryList = shouldClearHistoryList;
    }

    /** Returns whether session history should be cleared once the navigation commits. */
    public boolean getShouldClearHistoryList() {
        return mShouldClearHistoryList;
    }

    public boolean isBaseUrlDataScheme() {
        // If there's no base url set, but this is a data load then
        // treat the scheme as data:.
        if (mBaseUrlForDataUrl == null && mLoadUrlType == LoadURLType.DATA) {
            return true;
        }
        return LoadUrlParamsJni.get().isDataScheme(mBaseUrlForDataUrl);
    }

    @NativeMethods
    interface Natives {
        /**
         * Parses |url| as a GURL on the native side, and
         * returns true if it's scheme is data:.
         */
        boolean isDataScheme(String url);
    }
}
