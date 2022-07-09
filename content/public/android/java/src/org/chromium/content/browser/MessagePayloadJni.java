// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content_public.browser.MessagePayload;

/**
 * Helper class to call MessagePayload methods from native.
 */
@JNINamespace("content")
final class MessagePayloadJni {
    private MessagePayloadJni() {}

    @CalledByNative
    private static MessagePayload createFromString(String string) {
        return new MessagePayload(string);
    }

    @CalledByNative
    private static String getAsString(MessagePayload payload) {
        return payload.getAsString();
    }
}