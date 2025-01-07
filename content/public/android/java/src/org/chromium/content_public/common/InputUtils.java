// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.common;

import android.os.Build;

import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.ContentFeatureMap;

public class InputUtils {
    public static boolean isTransferInputToVizSupported() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM
                && ContentFeatureMap.isEnabled(ContentFeatureList.INPUT_ON_VIZ);
    }
}
