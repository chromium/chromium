// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content_public.browser;

import android.content.Context;
import android.os.Build;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;
import android.window.InputTransferToken;

import androidx.annotation.RequiresApi;

import org.jni_zero.CalledByNative;

import org.chromium.base.ContextUtils;
import org.chromium.base.TraceEvent;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;

@RequiresApi(Build.VERSION_CODES.VANILLA_ICE_CREAM)
@NullMarked
public class InputTransferHandler implements WindowAndroid.SelectionHandlesObserver {
    private static @Nullable Integer sInitialBrowserToken;

    private final InputTransferToken mBrowserToken;
    private @Nullable InputTransferToken mVizToken;
    private boolean mSelectionHandlesActive;
    private final WindowAndroid mWindowAndroid;

    public InputTransferHandler(InputTransferToken browserToken, WindowAndroid windowAndroid) {
        if (sInitialBrowserToken == null) {
            sInitialBrowserToken = browserToken.hashCode();
        }
        mBrowserToken = browserToken;
        mWindowAndroid = windowAndroid;
        mWindowAndroid.addSelectionHandlesObserver(this);
    }

    // WindowAndroid.SelectionHandlesObserver impl
    @Override
    public void onSelectionHandlesStateChanged(boolean active) {
        mSelectionHandlesActive = active;
    }

    public void destroy() {
        mWindowAndroid.removeSelectionHandlesObserver(this);
    }

    // TODO(crbug.com/393576167): Add integration tests for touch transfer cases.
    private @Nullable Integer canTransferInputToViz() {
        // To handle an early touch sequence, where Viz might not have sent back it's
        // TouchTransferToken back to Browser.
        // This also handles multi-window case, where Viz doesn't create InputReceiver for more than
        // one window and as a result `mVizToken` will be null.
        if (mVizToken == null) {
            if (SurfaceInputTransferHandlerMap.getMap().size() == 1) {
                return TransferInputToVizResult.VIZ_INITIALIZATION_NOT_COMPLETE;
            } else {
                return TransferInputToVizResult.MULTIPLE_BROWSER_WINDOWS_OPEN;
            }
        }

        // Browser InputTransfeToken might have changed. On Viz side we aren't destroying
        // InputReceiver (due to platform bug: b/385124056) which was created with the initial
        // Browser token. Attempt at transferring touch sequence using new Browser token and old Viz
        // token would fail, so just early out here instead of making a binder call later.
        assert sInitialBrowserToken != null;
        if (sInitialBrowserToken != mBrowserToken.hashCode()) {
            return TransferInputToVizResult.BROWSER_TOKEN_CHANGED;
        }

        // To prevent ordering issues between touch input and text selection commands. On Browser
        // side `FrameWidgetInputHandler` and `WidgetInputHandler` are associated, so this ordering
        // issue doesn't exists.
        if (mSelectionHandlesActive) {
            return TransferInputToVizResult.SELECTION_HANDLES_ACTIVE;
        }

        // To prevent ordering issues between touch input and ime input. For e.g. if Viz is allowed
        // to handle touch input while IME is active we could see cases like these: where user typed
        // something and then moved the cursor, it might reach renderer as touch input coming before
        // text i.e. the cursor moved and then typing happens, which would be contrary to what user
        // would have expected.
        InputMethodManager imm =
                (InputMethodManager)
                        ContextUtils.getApplicationContext()
                                .getSystemService(Context.INPUT_METHOD_SERVICE);
        if (imm.isAcceptingText()) {
            return TransferInputToVizResult.IME_IS_ACTIVE;
        }

        return null;
    }

    public void setVizToken(InputTransferToken token) {
        TraceEvent.instant("Storing InputTransferToken");
        mVizToken = token;
    }

    public @TransferInputToVizResult int transferInputToViz() {
        assert mVizToken != null;
        WindowManager wm =
                ContextUtils.getApplicationContext().getSystemService(WindowManager.class);
        if (wm.transferTouchGesture(mBrowserToken, mVizToken)) {
            return TransferInputToVizResult.SUCCESSFULLY_TRANSFERRED;
        } else {
            return TransferInputToVizResult.SYSTEM_SERVER_DID_NOT_TRANSFER;
        }
    }

    public @TransferInputToVizResult int maybeTransferInputToViz() {
        Integer noTransferReason = canTransferInputToViz();
        if (noTransferReason != null) {
            return noTransferReason;
        }
        return transferInputToViz();
    }

    @CalledByNative
    private static @TransferInputToVizResult int maybeTransferInputToViz(int surfaceId) {
        InputTransferHandler handler = SurfaceInputTransferHandlerMap.getMap().get(surfaceId);

        if (handler == null) {
            return TransferInputToVizResult.INPUT_TRANSFER_HANDLER_NOT_FOUND_IN_MAP;
        }

        return handler.maybeTransferInputToViz();
    }

    @CalledByNative
    private static @TransferInputToVizResult int transferInputToViz(int surfaceId) {
        InputTransferHandler handler = SurfaceInputTransferHandlerMap.getMap().get(surfaceId);

        if (handler == null) {
            return TransferInputToVizResult.INPUT_TRANSFER_HANDLER_NOT_FOUND_IN_MAP;
        }

        return handler.transferInputToViz();
    }
}
