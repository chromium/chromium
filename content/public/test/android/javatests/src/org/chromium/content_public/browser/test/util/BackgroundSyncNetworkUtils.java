// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser.test.util;

import androidx.annotation.VisibleForTesting;

import org.chromium.content.browser.BackgroundSyncNetworkObserver;
import org.chromium.net.ConnectionType;

/**
 * Used to mock network conditions for Background Sync.
 */
public class BackgroundSyncNetworkUtils {
    /**
     * Overrides connection type for testing.
     * @param connectionType The connectionType to override to. BackgroundSync code will be notified
     * of this connection type.
     */
    @VisibleForTesting
    public static void setConnectionTypeForTesting(@ConnectionType int connectionType) {
        BackgroundSyncNetworkObserver.setConnectionTypeForTesting(connectionType);
    }
}