// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.on_device_model;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * This is the Java representation of the C++ ml::InputPiece. It implements the equivalent
 * functionality of std::variant.
 */
@JNINamespace("on_device_model")
@NullMarked
public final class InputPiece {
    // LINT.IfChange(Token)
    @IntDef({Token.SYSTEM, Token.MODEL, Token.USER, Token.END})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Token {
        int SYSTEM = 0;
        int MODEL = 1;
        int USER = 2;
        int END = 3;
    }

    // LINT.ThenChange(//services/on_device_model/ml/chrome_ml_types.h:Token)

    @IntDef({InputPieceType.TEXT, InputPieceType.TOKEN})
    @Retention(RetentionPolicy.SOURCE)
    // LINT.IfChange(InputPieceType)
    private @interface InputPieceType {
        int TEXT = 0;
        int TOKEN = 1;
    }

    // LINT.ThenChange(//services/on_device_model/ml/chrome_ml_types.h:InputPiece)

    @InputPieceType private final int mType;
    @Nullable private final String mText;
    private final int mTokenId;

    private InputPiece(@InputPieceType int type, @Nullable String text, int tokenId) {
        mType = type;
        mText = text;
        mTokenId = tokenId;
    }

    @CalledByNative
    private static InputPiece createText(String text) {
        return new InputPiece(InputPieceType.TEXT, text, -1);
    }

    @CalledByNative
    private static InputPiece createToken(@Token int tokenId) {
        return new InputPiece(InputPieceType.TOKEN, null, tokenId);
    }

    public boolean isText() {
        return mType == InputPieceType.TEXT;
    }

    public boolean isToken() {
        return mType == InputPieceType.TOKEN;
    }

    public String getText() {
        assert isText() : "Cannot call getText() for a token InputPiece.";
        assert mText != null;
        return mText;
    }

    public @Token int getTokenId() {
        assert isToken() : "Cannot call getTokenId() for a text InputPiece.";
        return mTokenId;
    }
}
