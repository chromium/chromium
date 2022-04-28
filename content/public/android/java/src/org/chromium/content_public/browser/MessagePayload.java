// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Represents a JavaScript message payload.
 * Currently only STRING is supported.
 */
public final class MessagePayload {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef(value = {MessagePayloadType.TYPE_STRING})
    @interface MessagePayloadType {
        int TYPE_STRING = 0;
    }

    @MessagePayloadType
    private final int mType;
    @Nullable
    private final String mString;

    /**
     * Create a MessagePayload String type.
     * To keep backward compatibility, string can be null, then it's replaced to empty string in
     * JNI.
     */
    public MessagePayload(@Nullable String string) {
        mType = MessagePayloadType.TYPE_STRING;
        mString = string;
    }

    @MessagePayloadType
    public int getType() {
        return mType;
    }

    @Nullable
    public String getAsString() {
        if (mType != MessagePayloadType.TYPE_STRING) {
            throw new ClassCastException("String expected, but type is " + mType);
        }
        return mString;
    }
}
