// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import android.os.Build;
import android.window.InputTransferToken;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.content_public.browser.InputTransferHandler;
import org.chromium.content_public.browser.SurfaceInputTransferHandlerMap;

@NullMarked
public class InputTokenForwarderManager {
    @CalledByNative
    public static void onTokenReceived(int surfaceId, InputTransferToken vizInputToken) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.VANILLA_ICE_CREAM) {
            return;
        }
        InputTransferHandler handler = SurfaceInputTransferHandlerMap.getMap().get(surfaceId);
        if (handler != null) {
            handler.setVizToken(vizInputToken);
        }
    }
}
