// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import android.os.Build;
import android.view.WindowManager;
import android.window.InputTransferToken;

import androidx.annotation.RequiresApi;

import org.chromium.base.ContextUtils;
import org.chromium.base.TraceEvent;

@RequiresApi(Build.VERSION_CODES.VANILLA_ICE_CREAM)
public class InputTransferHandler {
    private InputTransferToken mBrowserToken;
    private InputTransferToken mVizToken;

    public InputTransferHandler(InputTransferToken browserToken) {
        mBrowserToken = browserToken;
    }

    private boolean canTransferInputToViz() {
        // TODO(370506271): Implement logic for when can we transfer vs not.
        return false;
    }

    public void setVizToken(InputTransferToken token) {
        TraceEvent.instant("Storing InputTransferToken");
        mVizToken = token;
    }

    public boolean maybeTransferInputToViz() {
        if (!canTransferInputToViz()) {
            return false;
        }
        WindowManager wm =
                ContextUtils.getApplicationContext().getSystemService(WindowManager.class);
        return wm.transferTouchGesture(mBrowserToken, mVizToken);
    }
}
