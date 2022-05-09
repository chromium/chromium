// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.geolocation;

/**
 * Set the MockLocationProvider to LocationProviderFactory. Used for test only.
 */
public final class LocationProviderOverrider {
    public static void setLocationProviderImpl(LocationProvider provider) {
        LocationProviderFactory.setLocationProviderImpl(provider);
    }

    private LocationProviderOverrider() {}
};
