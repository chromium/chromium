// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net;

import android.net.ConnectivityManager;

import org.chromium.build.annotations.NullMarked;

// Wrapper class for ConnectivityManager hidden APIs, which are not available outside the Android
// Connectivity mainline module.
// This class will be replaced by the class with actual implementation that calls these hidden APIs
// when building Cronet for the Connectivity mainline module.
@NullMarked
public class ConnectivityManagerShim {
    public static void registerQuicConnectionClosePayload(
            final ConnectivityManager cm, final int socket, final byte[] payload) {}

    public static void unregisterQuicConnectionClosePayload(
            final ConnectivityManager cm, final int socket) {}
}
