// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.on_device_model;

import org.chromium.build.annotations.NullMarked;
import org.chromium.on_device_model.mojom.GenerateOptions;
import org.chromium.on_device_model.mojom.InputPiece;

/**
 * The backend interface for an on-device model session. This is implemented by the downstream
 * dependency.
 */
@NullMarked
public interface AiCoreSessionBackend {
    /**
     * Request to generate text. Response will be delivered via the responder.
     *
     * @param generateOptions The generate options to generate the response.
     * @param inputPieces The input pieces to generate the response.
     * @param responder The responder to send back the response.
     */
    void generate(
            GenerateOptions generateOptions, InputPiece[] inputPieces, SessionResponder responder);

    /**
     * Gets the size in tokens for the given input pieces.
     * TODO(crbug.com/479233872): Remove the default implementation which is compatibility shim for
     * downstream code once downstream code is updated.
     *
     * @param inputPieces The input pieces to measure.
     * @param responder The responder to receive the token count result.
     */
    default void getSizeInTokens(InputPiece[] inputPieces, SessionResponder responder) {
        responder.onSizeInTokensResult(0);
    }

    /**
     * Called when the native session is destroyed. The implementation class should not call
     * responder after this is called.
     */
    void onNativeDestroyed();
}
