// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import android.app.Activity;

import org.jni_zero.CalledByNative;

import org.chromium.base.Callback;
import org.chromium.device.nfc.NfcDelegate;

/**
 * A //content-specific implementation of the NfcDelegate interface. Maps NFC host IDs to their
 * corresponding NfcHost objects, allowing the NFC implementation to access the Activity of the
 * WebContents with which its requesting frame is associated.
 */
public class ContentNfcDelegate implements NfcDelegate {
    @CalledByNative
    private static ContentNfcDelegate create() {
        return new ContentNfcDelegate();
    }

    @Override
    public void trackActivityForHost(int hostId, Callback<Activity> callback) {
        NfcHost host = NfcHost.fromContextId(hostId);
        assert host != null : "The corresponding host should have been ready before NfcImpl starts";
        host.trackActivityChanges(callback);
    }

    @Override
    public void stopTrackingActivityForHost(int hostId) {
        NfcHost host = NfcHost.fromContextId(hostId);
        assert host != null : "Unexpected stop request to an already stopped host";
        // |host| will destroy itself.
        host.stopTrackingActivityChanges();
    }
}
