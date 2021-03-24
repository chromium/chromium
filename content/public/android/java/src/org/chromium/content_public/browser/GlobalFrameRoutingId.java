// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import java.util.Objects;

/**
 * Identifies a RenderFrameHost.
 * See the native equivalent: content::GlobalFrameRoutingId.
 */
public final class GlobalFrameRoutingId {
    // Note that this is an internal identifier, not the PID from the OS.
    private final int mChildId;
    private final int mFrameRoutingId;

    public GlobalFrameRoutingId(int childId, int frameRoutingId) {
        mChildId = childId;
        mFrameRoutingId = frameRoutingId;
    }

    public int childId() {
        return mChildId;
    }
    public int frameRoutingId() {
        return mFrameRoutingId;
    }

    @Override
    public boolean equals(Object obj) {
        if (!(obj instanceof GlobalFrameRoutingId)) {
            return false;
        }
        GlobalFrameRoutingId that = (GlobalFrameRoutingId) obj;
        return mChildId == that.mChildId && mFrameRoutingId == that.mFrameRoutingId;
    }

    @Override
    public int hashCode() {
        return Objects.hash(mChildId, mFrameRoutingId);
    }
}
