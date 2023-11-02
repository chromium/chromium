// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import android.app.PendingIntent;
import android.content.Intent;
import android.util.Pair;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.annotations.NativeMethods;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Reflects the Android parts of the C++ class <code>WebAuthenticationDelegate</code>.
 * <p>
 * The C++ class hangs off the <code>ContentClient</code> and so this class can be constructed
 * without arguments on the assumption that construction happens after the global
 * <code>ContentClient</code> has been set.
 */
public class WebAuthenticationDelegate {
    private final long mNativeDelegate;

    public WebAuthenticationDelegate() {
        mNativeDelegate = WebAuthenticationDelegateJni.get().getNativeDelegate();
    }

    /**
     * Abstracts the task of starting an intent and getting the result.
     * <p>
     * This interface is ultimately expected to call Android's
     * <code>startIntentSenderForResult</code> and pass the resulting response code and {@link
     * Intent} to the given callback.
     */
    public interface IntentSender {
        /**
         * @param intent the intent to be started to complete a WebAuthn operation.
         * @param callback receives the response code and {@link Intent} resulting from the starting
         *         the {@link PendingIntent}.
         * @return true to indicate that the {@link PendingIntent} was started and false if it could
         *         not be.
         */
        boolean showIntent(PendingIntent intent, Callback<Pair<Integer, Intent>> callback);
    }

    /**
     * @return an {@link IntentSender} for starting WebAuthn Intents.
     */
    @Nullable
    public IntentSender getIntentSender(WebContents webContents) {
        return WebAuthenticationDelegateJni.get().getIntentSender(mNativeDelegate, webContents);
    }

    /**
     * Enumerates the different level of WebAuthn support in an Android app.
     */
    @IntDef({Support.NONE, Support.APP, Support.BROWSER})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Support {
        /**
         * WebAuthn is disabled. The <code>PublicKeyCredential</code> will be missing in Javascript
         * so that sites can detect that there's no WebAuthn support.
         */
        int NONE = 0;
        /**
         * The unprivileged Play Services API will be used. This requires that the app be listed in
         * the Asset Links for the RP ID that it is trying to assert / register for.
         */
        int APP = 1;
        /**
         * The privileged Play Services API will be used which allows any RP ID.
         */
        int BROWSER = 2;
    }

    /**
     * @return the current WebAuthn support level for the given {@link WebContents}.
     */
    public @Support int getSupportLevel(WebContents webContents) {
        return WebAuthenticationDelegateJni.get().getSupportLevel(mNativeDelegate, webContents);
    }

    @NativeMethods
    interface Natives {
        long getNativeDelegate();
        IntentSender getIntentSender(long delegatePtr, WebContents webContents);
        int getSupportLevel(long delegatePtr, WebContents webContents);
    }
}
