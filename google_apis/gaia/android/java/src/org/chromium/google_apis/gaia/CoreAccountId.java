// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.google_apis.gaia;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;

/**
 * Represents the id of an account, which on Android is always a Gaia ID. This class has a native
 * counterpart called CoreAccountId, which on platforms other than Android (namely ChromeOS) may
 * also be an e-mail.
 */
@NullMarked
public class CoreAccountId {
    private final GaiaId mId;

    /** Constructs a new CoreAccountId from a Gaia ID. */
    @CalledByNative
    public CoreAccountId(@JniType("GaiaId") GaiaId id) {
        assert id != null;
        mId = id;
    }

    @CalledByNative
    public @JniType("GaiaId") GaiaId getId() {
        return mId;
    }

    @Override
    public String toString() {
        return mId.toString();
    }

    @Override
    public int hashCode() {
        return mId.hashCode();
    }

    @Override
    public boolean equals(Object obj) {
        if (!(obj instanceof CoreAccountId)) return false;
        CoreAccountId other = (CoreAccountId) obj;
        return mId.equals(other.mId);
    }
}
