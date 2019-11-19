// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.common;

import android.os.Parcel;
import android.os.Parcelable;
import android.view.Surface;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;

/**
 * A wrapper for marshalling a Surface without self-destruction.
 */
@JNINamespace("content")
@MainDex
public class SurfaceWrapper implements Parcelable {
    private final Surface mSurface;
    private final boolean mCanBeUsedWithSurfaceControl;

    public SurfaceWrapper(Surface surface, boolean canBeUsedWithSurfaceControl) {
        mSurface = surface;
        mCanBeUsedWithSurfaceControl = canBeUsedWithSurfaceControl;
    }

    @CalledByNative
    public Surface getSurface() {
        return mSurface;
    }

    @CalledByNative
    public boolean canBeUsedWithSurfaceControl() {
        return mCanBeUsedWithSurfaceControl;
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel out, int flags) {
        // Ignore flags so that the Surface won't call release()
        mSurface.writeToParcel(out, 0);
        out.writeInt(mCanBeUsedWithSurfaceControl ? 1 : 0);
    }

    @CalledByNative
    private static SurfaceWrapper create(Surface surface, boolean canBeUsedWithSurfaceControl) {
        return new SurfaceWrapper(surface, canBeUsedWithSurfaceControl);
    }

    public static final Parcelable.Creator<SurfaceWrapper> CREATOR =
            new Parcelable.Creator<SurfaceWrapper>() {
                @Override
                public SurfaceWrapper createFromParcel(Parcel in) {
                    Surface surface = Surface.CREATOR.createFromParcel(in);
                    boolean canBeUsedWithSurfaceControl = (in.readInt() == 1);
                    return new SurfaceWrapper(surface, canBeUsedWithSurfaceControl);
                }

                @Override
                public SurfaceWrapper[] newArray(int size) {
                    return new SurfaceWrapper[size];
                }
            };
}
