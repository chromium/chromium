// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webxr;

import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.webxr.XrSessionCoordinator.SessionType;

/**
 * This class provides methods to interact with and query the state of any Xr specific runtimes.
 * Notably, it serves as a "wrapper" to abstract and coordinate any AR or VR specific states. It
 * will only be compiled into Chrome if either |enable_arcore| or |enable_cardboard| are set, and
 * attempts to load the relevant concrete implementations for the various XR Runtime Delegate
 * interfaces.
 */
@NullMarked
public class XrDelegateImpl implements XrDelegate {
    private @SessionType int mActiveSession = SessionType.NONE;

    private final SettableNonNullObservableSupplier<Boolean> mHasActiveSessionSupplier =
            ObservableSuppliers.createNonNull(false);

    public XrDelegateImpl() {
        XrSessionCoordinator.getActiveSessionTypeSupplier()
                .addSyncObserverAndPostIfNonNull(this::setActiveSessionType);
    }

    private void setActiveSessionType(@SessionType int sessionType) {
        boolean hasActiveSession = (mActiveSession != SessionType.NONE);
        boolean nowHasActiveSession = (sessionType != SessionType.NONE);
        mActiveSession = sessionType;
        if (hasActiveSession != nowHasActiveSession) {
            mHasActiveSessionSupplier.set(nowHasActiveSession);
        }
    }

    @Override
    public boolean onBackPressed() {
        return XrSessionCoordinator.endActiveSession();
    }

    @Override
    public @BackPressResult int handleBackPress() {
        return onBackPressed() ? BackPressResult.SUCCESS : BackPressResult.FAILURE;
    }

    @Override
    public NonNullObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mHasActiveSessionSupplier;
    }

    @Override
    public boolean hasActiveArSession() {
        return mActiveSession == SessionType.AR;
    }
}
