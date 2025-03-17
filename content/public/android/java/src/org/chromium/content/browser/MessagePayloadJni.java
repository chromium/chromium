// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.MessagePayload;
import org.chromium.content_public.browser.MessagePayloadType;

/** Helper class to call MessagePayload methods from native. */
@JNINamespace("content")
@NullMarked
final class MessagePayloadJni {
    private MessagePayloadJni() {}

    @MessagePayloadType
    @CalledByNative
    private static int getType(MessagePayload payload) {
        return payload.getType();
    }

    @CalledByNative
    private static MessagePayload createFromString(@Nullable String string) {
        return new MessagePayload(string);
    }

    @CalledByNative
    private static @Nullable String getAsString(MessagePayload payload) {
        return payload.getAsString();
    }

    @CalledByNative
    private static MessagePayload createFromArrayBuffer(byte[] arrayBuffer) {
        return new MessagePayload(arrayBuffer);
    }

    @CalledByNative
    private static byte[] getAsArrayBuffer(MessagePayload payload) {
        return payload.getAsArrayBuffer();
    }
}
