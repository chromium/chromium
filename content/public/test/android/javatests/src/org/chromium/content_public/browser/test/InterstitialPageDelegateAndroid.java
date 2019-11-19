// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content_public.browser.WebContents;

/**
 * Allows the specification and handling of Interstitial pages in java.
 */
@JNINamespace("content")
public class InterstitialPageDelegateAndroid {
    private long mNativePtr;

    /**
     * Constructs an interstitial with the given HTML content.
     *
     * @param htmlContent The HTML content for the interstitial.
     */
    @VisibleForTesting
    public InterstitialPageDelegateAndroid(String htmlContent) {
        mNativePtr = nativeInit(htmlContent);
    }

    /**
     * Called when "proceed" is triggered on the interstitial.
     */
    @CalledByNative
    protected void onProceed() {}

    /**
     * Called when "dont' proceed" is triggered on the interstitial.
     */
    @CalledByNative
    protected void onDontProceed() {}

    /**
     * Called when a command has been received from the interstitial.
     *
     * @param command The command that was received.
     */
    @CalledByNative
    protected void commandReceived(String command) {}

    @CalledByNative
    private void onNativeDestroyed() {
        mNativePtr = 0;
    }

    /**
     * Notifies the native interstitial to proceed.
     */
    protected void proceed() {
        if (mNativePtr != 0) nativeProceed(mNativePtr);
    }

    /**
     * Notifies the native interstitial to not proceed.
     */
    protected void dontProceed() {
        if (mNativePtr != 0) nativeDontProceed(mNativePtr);
    }

    /**
     * Shows an interstitial page driven by this delegate.
     *
     * @param url The URL being blocked by the interstitial.
     * @param webContents The {@link WebContents} the interstitial to show on.
     */
    public void showInterstitialPage(String url, WebContents webContents) {
        if (mNativePtr != 0) nativeShowInterstitialPage(mNativePtr, url, webContents);
    }

    private native long nativeInit(String htmlContent);
    private native void nativeProceed(long nativeInterstitialPageDelegateAndroid);
    private native void nativeDontProceed(long nativeInterstitialPageDelegateAndroid);
    private native void nativeShowInterstitialPage(
            long nativeInterstitialPageDelegateAndroid, String url, WebContents webContents);
}
