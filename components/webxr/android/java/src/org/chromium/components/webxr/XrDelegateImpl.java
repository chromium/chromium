// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webxr;

import androidx.annotation.IntDef;

import org.chromium.base.Log;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * This class provides methods to interact with and query the state of any Xr
 * specific runtimes. Notably, it serves as a "wrapper" to abstract and
 * coordinate any AR or VR specific states. It will only be compiled into Chrome
 * if either |enable_arcore| or |enable_cardboard| are set, and attempts to load
 * the relevant concrete implementations for the various XR Runtime Delegate
 * interfaces.
 */
public class XrDelegateImpl implements XrDelegate {
    private static final String TAG = "XrDelegateImpl";
    private static final boolean DEBUG_LOGS = false;

    @IntDef({SessionType.NONE, SessionType.AR, SessionType.VR})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SessionType {
        int NONE = 0;
        int AR = 1;
        int VR = 2;
    }

    static <T> T getDelegate(String className) {
        T delegate = null;
        try {
            delegate = (T) Class.forName(className).newInstance();
        } catch (ClassNotFoundException e) {
        } catch (InstantiationException e) {
        } catch (IllegalAccessException e) {
        }

        if (DEBUG_LOGS) {
            Log.i(TAG,
                    "getDelegate for class: " + className + " got delegate? " + (delegate == null));
        }
        return delegate;
    }
    // The ArDelegate is either included in the build or not, so it's okay to
    // cache an instance of it.
    private ArDelegate mArDelegate;

    private @XrDelegateImpl.SessionType int mActiveSession = XrDelegateImpl.SessionType.NONE;

    private ObservableSupplierImpl<Boolean> mHasActiveSessionSupplier =
            new ObservableSupplierImpl<>();

    public XrDelegateImpl() {
        mArDelegate = getDelegate("org.chromium.components.webxr.ArDelegateImpl");
        if (mArDelegate != null) {
            mArDelegate.getHasActiveArSessionSupplier().addObserver(this::setHasActiveArSession);
        }
    }

    private void setHasActiveArSession(Boolean hasSession) {
        if (hasSession) {
            assert (mActiveSession == XrDelegateImpl.SessionType.NONE);
            mActiveSession = XrDelegateImpl.SessionType.AR;
            mHasActiveSessionSupplier.set(true);
        } else if (mActiveSession == XrDelegateImpl.SessionType.AR) {
            mActiveSession = XrDelegateImpl.SessionType.NONE;
            mHasActiveSessionSupplier.set(false);
        }
    }

    @Override
    public boolean onBackPressed() {
        if (mActiveSession == XrDelegateImpl.SessionType.AR) {
            // If we have an active AR session we must have an ArDelegate.
            return mArDelegate.onBackPressed();
        }

        return false;
    }

    @Override
    public @BackPressResult int handleBackPress() {
        return onBackPressed() ? BackPressResult.SUCCESS : BackPressResult.FAILURE;
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mHasActiveSessionSupplier;
    }

    @Override
    public boolean hasActiveArSession() {
        return mActiveSession == XrDelegateImpl.SessionType.AR;
    }
}
