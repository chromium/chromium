// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.util;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnPageStartedHelper;
import org.chromium.url.GURL;

/**
 * The default WebContentsObserver used by ContentView tests. The below callbacks can be
 * accessed by using {@link TestCallbackHelperContainer} or extending this class.
 */
public class TestWebContentsObserver extends WebContentsObserver {
    private final OnPageStartedHelper mOnPageStartedHelper;
    private final OnPageFinishedHelper mOnPageFinishedHelper;
    private final CallbackHelper mOnFirstVisuallyNonEmptyPaintHelper;

    public TestWebContentsObserver(WebContents webContents) {
        super(webContents);
        mOnPageStartedHelper = new OnPageStartedHelper();
        mOnPageFinishedHelper = new OnPageFinishedHelper();
        mOnFirstVisuallyNonEmptyPaintHelper = new CallbackHelper();
    }

    public OnPageStartedHelper getOnPageStartedHelper() {
        return mOnPageStartedHelper;
    }

    public OnPageFinishedHelper getOnPageFinishedHelper() {
        return mOnPageFinishedHelper;
    }

    public CallbackHelper getOnFirstVisuallyNonEmptyPaintHelper() {
        return mOnFirstVisuallyNonEmptyPaintHelper;
    }

    /**
     * ATTENTION!: When overriding the following methods, be sure to call
     * the corresponding methods in the super class. Otherwise
     * {@link CallbackHelper#waitForCallback()} methods will
     * stop working!
     */
    @Override
    public void didStartLoading(GURL url) {
        super.didStartLoading(url);
        mOnPageStartedHelper.notifyCalled(url.getPossiblyInvalidSpec());
    }

    @Override
    public void didStopLoading(GURL url, boolean isKnownValid) {
        super.didStopLoading(url, isKnownValid);
        mOnPageFinishedHelper.notifyCalled(url.getPossiblyInvalidSpec());
    }

    @Override
    public void didFirstVisuallyNonEmptyPaint() {
        super.didFirstVisuallyNonEmptyPaint();
        mOnFirstVisuallyNonEmptyPaintHelper.notifyCalled();
    }
}
