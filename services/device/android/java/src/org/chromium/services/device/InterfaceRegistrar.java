// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.services.device;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.device.battery.BatteryMonitorFactory;
import org.chromium.device.mojom.BatteryMonitor;
import org.chromium.device.mojom.NfcProvider;
import org.chromium.device.mojom.VibrationManager;
import org.chromium.device.nfc.NfcDelegate;
import org.chromium.device.nfc.NfcProviderImpl;
import org.chromium.device.vibration.VibrationManagerImpl;
import org.chromium.mojo.system.impl.CoreImpl;
import org.chromium.services.service_manager.InterfaceRegistry;

@JNINamespace("device")
class InterfaceRegistrar {
    @CalledByNative
    static void createInterfaceRegistryForContext(
            int nativeHandle, NfcDelegate nfcDelegate) {
        // Note: The bindings code manages the lifetime of this object, so it
        // is not necessary to hold on to a reference to it explicitly.
        InterfaceRegistry registry = InterfaceRegistry.create(
                CoreImpl.getInstance().acquireNativeHandle(nativeHandle).toMessagePipeHandle());
        registry.addInterface(BatteryMonitor.MANAGER, new BatteryMonitorFactory());
        registry.addInterface(NfcProvider.MANAGER, new NfcProviderImpl.Factory(nfcDelegate));
        registry.addInterface(VibrationManager.MANAGER, new VibrationManagerImpl.Factory());
    }
}
