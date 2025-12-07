// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.on_device_model;

import org.chromium.build.annotations.NullMarked;
import org.chromium.on_device_model.mojom.GenerateOptions;
import org.chromium.on_device_model.mojom.InputPiece;

/** A dummy implementation of AiCoreSessionBackend. Used when AiCore is not available. */
@NullMarked
class AiCoreSessionBackendUpstreamImpl implements AiCoreSessionBackend {
    @Override
    public void generate(
            GenerateOptions generateOptions, InputPiece[] inputPieces, SessionResponder responder) {
        responder.onComplete(GenerateResult.API_NOT_AVAILABLE);
    }

    @Override
    public void onNativeDestroyed() {}
}
