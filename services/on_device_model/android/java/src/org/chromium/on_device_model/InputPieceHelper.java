// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.on_device_model;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;
import org.chromium.on_device_model.mojom.InputPiece;

@JNINamespace("on_device_model")
@NullMarked
class InputPieceHelper {
    @CalledByNative
    private static InputPiece fromToken(int token) {
        InputPiece inputPiece = new InputPiece();
        inputPiece.setToken(token);
        return inputPiece;
    }

    @CalledByNative
    private static InputPiece fromText(String text) {
        InputPiece inputPiece = new InputPiece();
        inputPiece.setText(text);
        return inputPiece;
    }
}
