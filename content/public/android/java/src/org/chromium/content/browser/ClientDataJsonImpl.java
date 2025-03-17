// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.blink.mojom.PaymentOptions;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.ClientDataRequestType;
import org.chromium.url.Origin;

import java.nio.ByteBuffer;

/** The implementation of ClientDataJson. */
@JNINamespace("content")
@NullMarked
public class ClientDataJsonImpl {
    /** The implementation of {@link ClientDataJson#buildClientDataJson}. */
    public static @Nullable String buildClientDataJson(
            @ClientDataRequestType int clientDataRequestType,
            String callerOrigin,
            byte[] challenge,
            boolean isCrossOrigin,
            @Nullable PaymentOptions paymentOptions,
            @Nullable String relyingPartyId,
            @Nullable Origin topOrigin) {
        return ClientDataJsonImplJni.get()
                .buildClientDataJson(
                        clientDataRequestType,
                        callerOrigin,
                        challenge,
                        isCrossOrigin,
                        paymentOptions == null ? null : paymentOptions.serialize(),
                        relyingPartyId,
                        topOrigin);
    }

    @NativeMethods
    public interface Natives {
        String buildClientDataJson(
                @ClientDataRequestType int clientDataRequestType,
                String callerOrigin,
                byte[] challenge,
                boolean isCrossOrigin,
                @Nullable ByteBuffer optionsByteBuffer,
                @Nullable String relyingPartyId,
                @Nullable Origin topOrigin);
    }
}
