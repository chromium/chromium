// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting.test.util;

/**
 * A container of a reference to pass data into or out from a closure. This class is usually useful
 * in test cases.
 *
 * @param <T> The type of reference.
 */
public class MutableReference<T> {
    private T mRef;

    public MutableReference() {
        clear();
    }

    public MutableReference(T ref) {
        set(ref);
    }

    public void clear() {
        set(null);
    }

    public void set(T ref) {
        mRef = ref;
    }

    public T get() {
        return mRef;
    }
}
