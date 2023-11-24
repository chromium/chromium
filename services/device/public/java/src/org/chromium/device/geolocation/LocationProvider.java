// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.geolocation;

/** LocationProvider interface. */
public interface LocationProvider {
    /**
     * Start listening for location updates. Calling several times before stop() is interpreted
     * as restart.
     * @param enableHighAccuracy Whether or not to enable high accuracy location.
     */
    public void start(boolean enableHighAccuracy);

    /** Stop listening for location updates. */
    public void stop();

    /** Returns true if we are currently listening for location updates, false if not. */
    public boolean isRunning();
}
