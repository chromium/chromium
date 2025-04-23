// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webxr;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.WebContents;

/**
 * This is the "stub" version of ArClassProvider and is only compiled in if |enable_arcore| is
 * false. As such, none of the concrete implementations for the exposed interfaces are present, and
 * this just returns null for all of the types. Any method signatures updated in the "real" version
 * need to also be updated here as well.
 */
@NullMarked
/*package*/ class ArClassProvider {
    /*package*/ static XrImmersiveOverlay.@Nullable Delegate getOverlayDelegate(
            ArCompositorDelegate compositorDelegate,
            final WebContents webContents,
            boolean useOverlay,
            boolean canRenderDomContent) {
        return null;
    }
}
