// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.common;

import android.annotation.SuppressLint;
import android.os.Build;
import android.os.Parcel;
import android.os.Parcelable;
import android.view.Surface;
import android.view.SurfaceControl;
import android.window.InputTransferToken;

import androidx.annotation.RequiresApi;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

/** A wrapper for marshalling a Surface without self-destruction. */
@JNINamespace("content")
public class SurfaceWrapper implements Parcelable {
    private final boolean mWrapsSurface;
    private Surface mSurface;
    private final boolean mCanBeUsedWithSurfaceControl;
    private SurfaceControl mSurfaceControl;
    private InputTransferToken mBrowserInputToken;

    private SurfaceWrapper(Surface surface, boolean canBeUsedWithSurfaceControl) {
        mWrapsSurface = true;
        mSurface = surface;
        mCanBeUsedWithSurfaceControl = canBeUsedWithSurfaceControl;
        mSurfaceControl = null;
        mBrowserInputToken = null;
    }

    private SurfaceWrapper(
            Surface surface,
            boolean canBeUsedWithSurfaceControl,
            InputTransferToken browserInputToken) {
        this(surface, canBeUsedWithSurfaceControl);
        assert browserInputToken != null;
        mBrowserInputToken = browserInputToken;
    }

    @RequiresApi(Build.VERSION_CODES.Q)
    private SurfaceWrapper(SurfaceControl surfaceControl) {
        mWrapsSurface = false;
        mSurface = null;
        mCanBeUsedWithSurfaceControl = true;
        mSurfaceControl = surfaceControl;
        mBrowserInputToken = null;
    }

    @CalledByNative
    public InputTransferToken getBrowserInputToken() {
        return mBrowserInputToken;
    }

    @CalledByNative
    private boolean getWrapsSurface() {
        return mWrapsSurface;
    }

    @CalledByNative
    private Surface takeSurface() {
        assert mWrapsSurface;
        Surface surface = mSurface;
        mSurface = null;
        return surface;
    }

    @CalledByNative
    private boolean canBeUsedWithSurfaceControl() {
        return mCanBeUsedWithSurfaceControl;
    }

    @CalledByNative
    private SurfaceControl takeSurfaceControl() {
        assert !mWrapsSurface;
        SurfaceControl sc = mSurfaceControl;
        mSurfaceControl = null;
        return sc;
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @SuppressLint("NewApi")
    private void writeInputTokenToParcel(Parcel out) {
        assert mBrowserInputToken != null;
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM;
        mBrowserInputToken.writeToParcel(out, 0);
    }

    @Override
    public void writeToParcel(Parcel out, int flags) {
        out.writeInt(mWrapsSurface ? 1 : 0);
        if (mWrapsSurface) {
            boolean hasBrowserInputToken = mBrowserInputToken != null;
            out.writeInt(hasBrowserInputToken ? 1 : 0);
            // Ignore flags so that the Surface won't call release()
            mSurface.writeToParcel(out, 0);
            out.writeInt(mCanBeUsedWithSurfaceControl ? 1 : 0);
            if (hasBrowserInputToken) {
                writeInputTokenToParcel(out);
            }
        } else {
            // Ignore flags so that SurfaceControl won't call release().
            mSurfaceControl.writeToParcel(out, 0);
        }
    }

    @RequiresApi(Build.VERSION_CODES.VANILLA_ICE_CREAM)
    @CalledByNative
    private static SurfaceWrapper create(
            Surface surface,
            boolean canBeUsedWithSurfaceControl,
            InputTransferToken browserInputToken) {
        return new SurfaceWrapper(surface, canBeUsedWithSurfaceControl, browserInputToken);
    }

    @CalledByNative
    private static SurfaceWrapper create(Surface surface, boolean canBeUsedWithSurfaceControl) {
        return new SurfaceWrapper(surface, canBeUsedWithSurfaceControl);
    }

    @RequiresApi(Build.VERSION_CODES.Q)
    @CalledByNative
    private static SurfaceWrapper createFromSurfaceControl(SurfaceControl surfaceControl) {
        return new SurfaceWrapper(surfaceControl);
    }

    @SuppressLint("NewApi")
    private static InputTransferToken createInputTokenFromParcel(Parcel in) {
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM;
        InputTransferToken token = InputTransferToken.CREATOR.createFromParcel(in);
        assert token != null;
        return token;
    }

    public static final Parcelable.Creator<SurfaceWrapper> CREATOR =
            new Parcelable.Creator<SurfaceWrapper>() {
                @Override
                public SurfaceWrapper createFromParcel(Parcel in) {
                    final boolean wrapsSurface = (in.readInt() == 1);
                    if (wrapsSurface) {
                        final boolean hasBrowserInputToken = (in.readInt() == 1);
                        Surface surface = Surface.CREATOR.createFromParcel(in);
                        boolean canBeUsedWithSurfaceControl = (in.readInt() == 1);
                        if (hasBrowserInputToken) {
                            InputTransferToken browserInputToken = createInputTokenFromParcel(in);
                            return new SurfaceWrapper(
                                    surface, canBeUsedWithSurfaceControl, browserInputToken);
                        }
                        return new SurfaceWrapper(surface, canBeUsedWithSurfaceControl);
                    } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                        SurfaceControl surfaceControl = SurfaceControl.CREATOR.createFromParcel(in);
                        return new SurfaceWrapper(surfaceControl);
                    } else {
                        throw new RuntimeException("not reached");
                    }
                }

                @Override
                public SurfaceWrapper[] newArray(int size) {
                    return new SurfaceWrapper[size];
                }
            };
}
