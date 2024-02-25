// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webauthn;

import android.app.PendingIntent;
import android.content.Intent;
import android.util.Pair;

import org.chromium.base.Callback;

/**
 * Abstracts the task of starting an intent and getting the result.
 *
 * <p>This interface is ultimately expected to call Android's <code>startIntentSenderForResult
 * </code> and pass the resulting response code and {@link Intent} to the given callback.
 */
public interface FidoIntentSender {
    /**
     * @param intent the intent to be started to complete a WebAuthn operation.
     * @param callback receives the response code and {@link Intent} resulting from the starting the
     *     {@link PendingIntent}.
     * @return true to indicate that the {@link PendingIntent} was started and false if it could not
     *     be.
     */
    boolean showIntent(PendingIntent intent, Callback<Pair<Integer, Intent>> callback);
}
