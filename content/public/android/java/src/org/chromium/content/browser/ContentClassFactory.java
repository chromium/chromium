// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser;

import org.chromium.base.ThreadUtils;

/** A class factory for downstream injecting code to content layer. */
public class ContentClassFactory {
    private static ContentClassFactory sSingleton;

    /** Sets the factory object. */
    public static void set(ContentClassFactory factory) {
        ThreadUtils.assertOnUiThread();

        sSingleton = factory;
    }

    /** Returns the factory object. */
    public static ContentClassFactory get() {
        ThreadUtils.assertOnUiThread();

        if (sSingleton == null) sSingleton = new ContentClassFactory();
        return sSingleton;
    }

    /** Constructor. */
    protected ContentClassFactory() {}
}
