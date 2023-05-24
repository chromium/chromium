// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import org.chromium.base.UnguessableToken;

/**
 * Holds parameters for NavigationController::LoadUrlParams::Impression. This is used to route
 * Attribution Reporting API parameters on navigations.
 */
public class Impression {
    private final UnguessableToken mAttributionSrcToken;
    private final UnguessableToken mInitiatorFrameToken;
    private final long mAttributionRuntimeFeatures;
    private final int mInitiatorProcessID;

    public Impression(UnguessableToken attributionSrcToken, UnguessableToken initiatorFrameToken,
            int initiatorProcessID, long attributionRuntimeFeatures) {
        mAttributionSrcToken = attributionSrcToken;
        mInitiatorFrameToken = initiatorFrameToken;
        mInitiatorProcessID = initiatorProcessID;
        mAttributionRuntimeFeatures = attributionRuntimeFeatures;
    }

    public UnguessableToken getAttributionSrcToken() {
        return mAttributionSrcToken;
    }

    public UnguessableToken getInitiatorFrameToken() {
        return mInitiatorFrameToken;
    }

    public int getInitiatorProcessID() {
        return mInitiatorProcessID;
    }

    public long getAttributionRuntimeFeatures() {
        return mAttributionRuntimeFeatures;
    }
}
