// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.native_test;

import android.app.Activity;
import android.os.Bundle;

/**
 * An {@link android.app.Activity} for running native unit tests.
 * (i.e., not browser tests)
 */
public class NativeUnitTestActivity extends Activity {
    private NativeUnitTest mTest = new NativeUnitTest();

    @Override
    public void onCreate(Bundle savedInstanceState) {
        mTest.preCreate(this);
        super.onCreate(savedInstanceState);
        mTest.postCreate(this);
    }

    @Override
    public void onStart() {
        super.onStart();
        mTest.postStart(this, false);
    }
}
