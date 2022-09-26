// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.gms.shadows;

import android.content.Context;

import com.google.android.gms.common.GoogleApiAvailability;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.gms.ChromiumPlayServicesAvailability;

@Implements(ChromiumPlayServicesAvailability.class)
public class ShadowChromiumPlayServicesAvailability {
    private static boolean sChromiumSuccess;
    private static int sConnectionResult;

    public static void setIsGooglePlayServicesAvailable(boolean value) {
        sChromiumSuccess = value;
    }
    public static void setGetGooglePlayServicesConnectionResult(int value) {
        sConnectionResult = value;
    }

    @Implementation
    public static int getGooglePlayServicesConnectionResult(final Context context) {
        return sConnectionResult;
    }

    @Implementation
    public static boolean isGooglePlayServicesAvailable(final Context context) {
        return sChromiumSuccess;
    }
}
