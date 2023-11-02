// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.nfc;

import android.app.Activity;

import org.chromium.base.Callback;

/** Interface that allows the NFC implementation to access the Activity associated with a given
 * client. |hostId| is the same ID passed in NFCProvider::GetNFCForHost().
 */
public interface NfcDelegate {
    /** Calls |callback| with the Activity associated with |hostId|, and subsequently calls
     * |callback| again whenever the Activity associated with |hostId| changes.
     */
    void trackActivityForHost(int hostId, Callback<Activity> callback);

    /** Called when the NFC implementation no longer needs to track the Activity associated with
     * |hostId|.
     */
    void stopTrackingActivityForHost(int hostId);
}
