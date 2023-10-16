// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import android.app.PendingIntent;
import android.content.Intent;
import android.util.Pair;

import androidx.annotation.Nullable;

import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;

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

    @NativeMethods
    interface Natives {
        long getNativeDelegate();
        IntentSender getIntentSender(long delegatePtr, WebContents webContents);
    }
}
