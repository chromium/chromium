// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromoting;

/** The parameter for an InputModeChanged event. */
public final class InputModeChangedEventParameter {
    public final @Desktop.InputMode int inputMode;
    public final @CapabilityManager.HostCapability int hostCapability;

    public InputModeChangedEventParameter(@Desktop.InputMode int inputMode,
            @CapabilityManager.HostCapability int hostCapability) {
        this.inputMode = inputMode;
        this.hostCapability = hostCapability;
    }
}
