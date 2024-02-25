// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webxr;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.content_public.browser.WebContents;

/**
 * This is the "stub" version of ArClassProvider and is only compiled in if |enable_arcore| is
 * false. As such, none of the concrete implementations for the exposed interfaces are present, and
 * this just returns null for all of the types. Any method signatures updated in the "real" version
 * need to also be updated here as well.
 */
/*package*/ class ArClassProvider {
    /*package*/ static @Nullable XrImmersiveOverlay.Delegate getOverlayDelegate(
            @NonNull ArCompositorDelegate compositorDelegate,
            @NonNull final WebContents webContents,
            boolean useOverlay,
            boolean canRenderDomContent) {
        return null;
    }
}
