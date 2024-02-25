// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import java.util.Objects;

/**
 * Identifies a RenderFrameHost.
 * See the native equivalent: content::GlobalRenderFrameHostId.
 */
public final class GlobalRenderFrameHostId {
    // Note that this is an internal identifier, not the PID from the OS.
    private final int mChildId;
    private final int mFrameRoutingId;

    public GlobalRenderFrameHostId(int childId, int frameRoutingId) {
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
        if (!(obj instanceof GlobalRenderFrameHostId)) {
            return false;
        }
        GlobalRenderFrameHostId that = (GlobalRenderFrameHostId) obj;
        return mChildId == that.mChildId && mFrameRoutingId == that.mFrameRoutingId;
    }

    @Override
    public int hashCode() {
        return Objects.hash(mChildId, mFrameRoutingId);
    }
}
