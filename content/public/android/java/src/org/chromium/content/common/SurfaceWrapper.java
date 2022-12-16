// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.common;

import android.os.Build;
import android.os.Parcel;
import android.os.Parcelable;
import android.view.Surface;
import android.view.SurfaceControl;

import androidx.annotation.RequiresApi;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.build.annotations.MainDex;

/**
 * A wrapper for marshalling a Surface without self-destruction.
 */
@JNINamespace("content")
@MainDex
public class SurfaceWrapper implements Parcelable {
    private final boolean mWrapsSurface;
    private Surface mSurface;
    private final boolean mCanBeUsedWithSurfaceControl;
    private SurfaceControl mSurfaceControl;

    private SurfaceWrapper(Surface surface, boolean canBeUsedWithSurfaceControl) {
        mWrapsSurface = true;
        mSurface = surface;
        mCanBeUsedWithSurfaceControl = canBeUsedWithSurfaceControl;
        mSurfaceControl = null;
    }

    @RequiresApi(Build.VERSION_CODES.Q)
    private SurfaceWrapper(SurfaceControl surfaceControl) {
        mWrapsSurface = false;
        mSurface = null;
        mCanBeUsedWithSurfaceControl = true;
        mSurfaceControl = surfaceControl;
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

    @Override
    public void writeToParcel(Parcel out, int flags) {
        out.writeInt(mWrapsSurface ? 1 : 0);
        if (mWrapsSurface) {
            // Ignore flags so that the Surface won't call release()
            mSurface.writeToParcel(out, 0);
            out.writeInt(mCanBeUsedWithSurfaceControl ? 1 : 0);
        } else {
            // Ignore flags so that SurfaceControl won't call release().
            mSurfaceControl.writeToParcel(out, 0);
        }
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

    public static final Parcelable.Creator<SurfaceWrapper> CREATOR =
            new Parcelable.Creator<SurfaceWrapper>() {
                @Override
                public SurfaceWrapper createFromParcel(Parcel in) {
                    final boolean wrapsSurface = (in.readInt() == 1);
                    if (wrapsSurface) {
                        Surface surface = Surface.CREATOR.createFromParcel(in);
                        boolean canBeUsedWithSurfaceControl = (in.readInt() == 1);
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
