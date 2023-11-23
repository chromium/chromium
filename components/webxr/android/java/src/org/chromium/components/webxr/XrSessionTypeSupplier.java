// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webxr;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.components.webxr.XrSessionCoordinator.SessionType;

/** A thin wrapper/subclass of ObservableSupplierImpl to add some type safety for the Xr SessionType. */
public class XrSessionTypeSupplier extends ObservableSupplierImpl<Integer> {
    public XrSessionTypeSupplier(@SessionType int initialValue) {
        set(initialValue);
    }

    @Override
    public void set(@SessionType Integer value) {
        assert value != null;
        super.set(value);
    }
}
