// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import androidx.annotation.Nullable;

import org.chromium.base.UnguessableToken;

/**
 * Holds parameters for NavigationController::LoadUrlParams::AdditionalNavigationParams. This is
 * used to route information about the initiator frame to the navigation request, which is needed
 * for event-level reporting to function properly.
 */
public class AdditionalNavigationParams {
    private final UnguessableToken mInitiatorFrameToken;
    private final int mInitiatorProcessId;

    // Parameters related to Attribution Reporting Impressions. May not always
    // be set.
    private final @Nullable UnguessableToken mAttributionSrcToken;

    public AdditionalNavigationParams(
            UnguessableToken initiatorFrameToken,
            int initiatorProcessId,
            @Nullable UnguessableToken attributionSrcToken) {
        mInitiatorFrameToken = initiatorFrameToken;
        mInitiatorProcessId = initiatorProcessId;
        mAttributionSrcToken = attributionSrcToken;
    }

    public UnguessableToken getInitiatorFrameToken() {
        return mInitiatorFrameToken;
    }

    public int getInitiatorProcessId() {
        return mInitiatorProcessId;
    }

    public @Nullable UnguessableToken getAttributionSrcToken() {
        return mAttributionSrcToken;
    }
}
