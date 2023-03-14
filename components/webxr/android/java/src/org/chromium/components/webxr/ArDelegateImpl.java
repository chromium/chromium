// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webxr;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.UsedByReflection;

/**
 * This class provides methods to call into AR. It will be compiled into Chrome
 * only if |enable_arcore| is set at build time.
 */
@UsedByReflection("XrDelegateImpl.java")
/*package*/ class ArDelegateImpl implements ArDelegate {
    @UsedByReflection("XrDelegateImpl.java")
    public ArDelegateImpl() {}

    @Override
    public boolean onBackPressed() {
        return ArCoreJavaUtils.onBackPressed();
    }

    @Override
    public ObservableSupplier<Boolean> getHasActiveArSessionSupplier() {
        return ArCoreJavaUtils.hasActiveArSessionSupplier();
    }
}
