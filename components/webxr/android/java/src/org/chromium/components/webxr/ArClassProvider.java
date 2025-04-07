// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webxr;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.WebContents;

/**
 * The "real" version of ArClassProvider, which is only built if |enable_arcore| is true, which
 * means that all of our dependent types are also compiled in and we can directly create them here.
 * Any method that you add or update the signature of here needs to also be updated in the
 * corresponding /stubs/ version of this class.
 */
@NullMarked
/*package*/ class ArClassProvider {
    /*package*/ static XrImmersiveOverlay.@Nullable Delegate getOverlayDelegate(
            ArCompositorDelegate compositorDelegate,
            final WebContents webContents,
            boolean useOverlay,
            boolean canRenderDomContent) {
        return new ArOverlayDelegate(
                compositorDelegate, webContents, useOverlay, canRenderDomContent);
    }
}
