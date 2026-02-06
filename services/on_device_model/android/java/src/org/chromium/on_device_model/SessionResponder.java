// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.on_device_model;

import org.chromium.build.annotations.NullMarked;

/** A responder for the backend to send responses. The functions can be called from any thread. */
@NullMarked
public interface SessionResponder {
    /**
     * Called with a piece of the response. This can be called multiple times. This is for the
     * generate function in AiCoreSessionBackend.
     *
     * @param response The response string.
     */
    void onResponse(String response);

    /**
     * Called when the response is complete. This is for the generate function in
     * AiCoreSessionBackend.
     *
     * @param result The result code of the generation.
     */
    void onComplete(@GenerateResult int result);

    /**
     * Called when the token count result is available. This is for the getSizeInTokens function in
     * AiCoreSessionBackend.
     *
     * @param tokenCount The number of tokens in the input.
     */
    void onSizeInTokensResult(int tokenCount);
}
