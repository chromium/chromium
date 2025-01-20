// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.framehost;

import org.chromium.build.annotations.NullMarked;

/**
 * The RenderFrameHost Java wrapper to allow communicating with the native RenderFrameHost object.
 *
 */
@NullMarked
public interface RenderFrameHostDelegate {
    // Mirrors callbacks for native RenderFrameHostDelegate.
    void renderFrameCreated(RenderFrameHostImpl host);

    void renderFrameDeleted(RenderFrameHostImpl host);
}
