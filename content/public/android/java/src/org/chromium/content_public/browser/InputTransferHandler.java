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
import org.chromium.ui.InsetObserver;
import org.chromium.ui.OverscrollRefreshHandler;
import org.chromium.ui.base.WindowAndroid;

@RequiresApi(Build.VERSION_CODES.VANILLA_ICE_CREAM)
@NullMarked
public class InputTransferHandler
        implements WindowAndroid.SelectionHandlesObserver, InsetObserver.WindowInsetObserver {
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
    // Insets provided by Android.
    private int mSystemGestureInsetLeft;
    private int mSystemGestureInsetRight;
    private @Nullable InsetObserver mInsetObserver;

    public InputTransferHandler(
            InputTransferToken browserToken, Delegate delegate, WindowAndroid windowAndroid) {
        mBrowserToken = browserToken;
        mDelegate = delegate;
        mWindowAndroid = windowAndroid;
        mWindowAndroid.addSelectionHandlesObserver(this);
        mInsetObserver = mWindowAndroid.getInsetObserver();
        if (mInsetObserver != null) {
            mInsetObserver.addObserver(this);
        }
    }

    /** WindowAndroid.SelectionHandlesObserver impl. */
    @Override
    public void onSelectionHandlesStateChanged(boolean active) {
        mSelectionHandlesActive = active;
    }

    /** InsetObserver.WindowInsetObserver impl. */
    @Override
    public void onSystemGestureInsetsChanged(int left, int top, int right, int bottom) {
        mSystemGestureInsetLeft = left;
        mSystemGestureInsetRight = right;
    }

    private boolean isWithinInsets(float x, float leftInset, float rightInset) {
        return x < leftInset || (mWindowAndroid.getDisplay().getDisplayWidth() - x < rightInset);
    }

    private boolean canTriggerBackGesture(float rawX) {
        boolean canTriggerSystemGesture =
                isWithinInsets(rawX, mSystemGestureInsetLeft, mSystemGestureInsetRight);
        float chromiumInsets =
                OverscrollRefreshHandler.DEFAULT_NAVIGATION_EDGE_WIDTH
                        * mWindowAndroid.getDisplay().getDipScale();
        // TODO(365985685): Remove once OverscrollController works with input being handled on Viz.
        boolean canTriggerChromiumGesture =
                (mSystemGestureInsetLeft == 0)
                        && isWithinInsets(rawX, chromiumInsets, chromiumInsets);
        return canTriggerSystemGesture || canTriggerChromiumGesture;
    }

    public void destroy() {
        mWindowAndroid.removeSelectionHandlesObserver(this);
        if (mInsetObserver != null) {
            mInsetObserver.removeObserver(this);
        }
        mDelegate.destroy();
    }

    // TODO(crbug.com/393576167): Add integration tests for touch transfer cases.
    private boolean canTransferInputToViz(float rawX) {
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

        // Do not transfer if this touch sequence could be converted into a system back or chromium
        // internal back gesture. When system takes over gesture it doesn't always provide a touch
        // cancel if the sequence was already on Viz. Chromium internal back uses
        // OverscrollController which doesn't get input when touch sequence is being handled on Viz.
        if (canTriggerBackGesture(rawX)) {
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

    public boolean maybeTransferInputToViz(float rawX) {
        if (!canTransferInputToViz(rawX)) {
            return false;
        }
        assert mVizToken != null;
        WindowManager wm =
                ContextUtils.getApplicationContext().getSystemService(WindowManager.class);
        return wm.transferTouchGesture(mBrowserToken, mVizToken);
    }

    @CalledByNative
    private static boolean maybeTransferInputToViz(int surfaceId, float rawX) {
        InputTransferHandler handler = SurfaceInputTransferHandlerMap.getMap().get(surfaceId);

        if (handler == null) {
            return false;
        }

        return handler.maybeTransferInputToViz(rawX);
    }
}
