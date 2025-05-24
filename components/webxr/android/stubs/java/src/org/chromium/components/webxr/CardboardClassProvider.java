// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webxr;

import android.app.Activity;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * This "stub" version of CardboardClassProvider is only built if |enable_cardboard| is false. As
 * such, none of the concrete implementations for the exposed interfaces are present, and this just
 * returns null for all of the types. Any method signatures updated in the "real" version need to
 * also be updated here as well.
 */
@NullMarked
/*package*/ class CardboardClassProvider {
    /*package*/ static XrImmersiveOverlay.@Nullable Delegate getOverlayDelegate(
            VrCompositorDelegate compositorDelegate, Activity activity) {
        return null;
    }
}
