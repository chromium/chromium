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
    // TODO(crbug.com/365985685): Remove `Delegate` once overscroll controller works with input
    // coming on Viz. Since that is the only use case it covers.
    public static interface Delegate {
        // To give embedders an option to stop input transfer to Viz.
        public default boolean canTransferInputToViz() {
            return true;
        }

        // Called before `InputTransferHandler` is being removed from
        // `SurfaceInputTransferHandlerMap`, giving the `Delegate` a chance to remove itself from
        // various observer queues.
        public default void destroy() {}
    }
    ;

    private InputTransferToken mBrowserToken;
    private @Nullable InputTransferToken mVizToken;
    private boolean mSelectionHandlesActive;
    private Delegate mDelegate;
    private WindowAndroid mWindowAndroid;

    public InputTransferHandler(
            InputTransferToken browserToken, Delegate delegate, WindowAndroid windowAndroid) {
        mBrowserToken = browserToken;
        mDelegate = delegate;
        mWindowAndroid = windowAndroid;
        mWindowAndroid.addSelectionHandlesObserver(this);
    }

    /** WindowAndroid.SelectionHandlesObserver impl. */
    @Override
    public void onSelectionHandlesStateChanged(boolean active) {
        mSelectionHandlesActive = active;
    }

    public void destroy() {
        mWindowAndroid.removeSelectionHandlesObserver(this);
        mDelegate.destroy();
    }

    private boolean canTransferInputToViz() {
        // To handle an early touch sequence, where Viz might not have sent back it's
        // TouchTransferToken back to Browser.
        // This also handles multi-window case, where Viz doesn't create InputReceiver for more than
        // one window and as a result `mVizToken` will be null.
        if (mVizToken == null) {
            return false;
        }

        // To prevent ordering issues between touch input and text selection commands. On Browser
        // side `FrameWidgetInputHandler` and `WidgetInputHandler` are associated, so this ordering
        // issue doesn't exists.
        if (mSelectionHandlesActive) {
            return false;
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
            return false;
        }

        // Give embedders an opportunity to decide when not to transfer.
        if (!mDelegate.canTransferInputToViz()) {
            return false;
        }

        return true;
    }

    public void setVizToken(InputTransferToken token) {
        TraceEvent.instant("Storing InputTransferToken");
        mVizToken = token;
    }

    public boolean maybeTransferInputToViz() {
        if (!canTransferInputToViz()) {
            return false;
        }
        assert mVizToken != null;
        WindowManager wm =
                ContextUtils.getApplicationContext().getSystemService(WindowManager.class);
        return wm.transferTouchGesture(mBrowserToken, mVizToken);
    }

    @CalledByNative
    private static boolean maybeTransferInputToViz(int surfaceId) {
        InputTransferHandler handler = SurfaceInputTransferHandlerMap.getMap().get(surfaceId);

        if (handler == null) {
            return false;
        }

        return handler.maybeTransferInputToViz();
    }
}
