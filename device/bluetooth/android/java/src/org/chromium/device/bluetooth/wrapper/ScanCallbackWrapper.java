// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.bluetooth.wrapper;

import org.chromium.build.annotations.NullMarked;

import java.util.List;

/**
 * Wraps android.bluetooth.le.ScanCallback, being called by ScanCallbackImpl.
 */
@NullMarked
public interface ScanCallbackWrapper {
    public void onBatchScanResult(List<ScanResultWrapper> results);

    public void onScanResult(int callbackType, ScanResultWrapper result);

    public void onScanFailed(int errorCode);
}
