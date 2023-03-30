// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webxr;

import android.app.Activity;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/**
 * This "real" version of CardboardClassProvider is only built if |enable_cardboard| is true, which
 * means that all of our dependent types are also compiled in and we can directly create them here.
 * Any method that you add or update the signature of here needs to also be updated in the
 * corresponding /stubs/ version of this class.
 */
/*package*/ class CardboardClassProvider {
    /*package*/ static @Nullable XrImmersiveOverlay.Delegate getOverlayDelegate(
            VrCompositorDelegate compositorDelegate, @NonNull Activity activity) {
        return new CardboardOverlayDelegate(compositorDelegate, activity);
    }
}
