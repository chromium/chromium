// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import android.os.Build;

import androidx.annotation.RequiresApi;

import org.chromium.build.annotations.NullMarked;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

@RequiresApi(Build.VERSION_CODES.VANILLA_ICE_CREAM)
@NullMarked
public class SurfaceInputTransferHandlerMap {
    public final Map<Integer, InputTransferHandler> mInputTransferHandlerMap =
            Collections.synchronizedMap(new HashMap<Integer, InputTransferHandler>());

    private SurfaceInputTransferHandlerMap() {}

    private static class LazyHolder {
        private static final SurfaceInputTransferHandlerMap INSTANCE =
                new SurfaceInputTransferHandlerMap();
    }

    public static Map<Integer, InputTransferHandler> getMap() {
        return LazyHolder.INSTANCE.mInputTransferHandlerMap;
    }

    public static void remove(int surfaceId) {
        InputTransferHandler handler = getMap().get(surfaceId);
        assert handler != null;
        handler.destroy();
        getMap().remove(surfaceId);
    }
}
