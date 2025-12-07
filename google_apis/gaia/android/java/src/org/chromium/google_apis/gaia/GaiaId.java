// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.google_apis.gaia;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;

/** Represents an account ID or Gaia ID. This class has a native counterpart also called GaiaId. */
@NullMarked
public class GaiaId {
    private final String mValue;

    /** Constructs a new GaiaId from a String representation of the gaia ID. */
    @CalledByNative
    public GaiaId(@JniType("std::string") String value) {
        assert value != null;
        mValue = value;
    }

    @Override
    @CalledByNative
    public @JniType("std::string") String toString() {
        // Note that the value returned here is used to marshal the native `GaiaId` class during the
        // conversion from Java to C++.
        return mValue;
    }

    @Override
    public int hashCode() {
        return mValue.hashCode();
    }

    @Override
    public boolean equals(Object obj) {
        if (!(obj instanceof GaiaId)) return false;
        GaiaId other = (GaiaId) obj;
        return mValue.equals(other.mValue);
    }
}
