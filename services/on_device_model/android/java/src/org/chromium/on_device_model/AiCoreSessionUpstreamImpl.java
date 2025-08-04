// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.on_device_model;

import org.chromium.build.annotations.NullMarked;

/** A dummy implementation of AiCoreSession. Used when AiCore is not available. */
@NullMarked
class AiCoreSessionUpstreamImpl implements AiCoreSession {
    @Override
    public void generate(long nativeBackendSession, Object[] inputPieces) {
        AiCoreSessionJni.get().onComplete(nativeBackendSession, GenerateResult.API_NOT_AVAILABLE);
    }

    @Override
    public void onNativeDestroyed() {
        // Do nothing because generate is handled synchronously.
    }
}
