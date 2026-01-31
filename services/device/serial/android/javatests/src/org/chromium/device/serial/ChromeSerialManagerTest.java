// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.serial;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.OutcomeReceiver;
import android.os.ParcelFileDescriptor;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.serial.SerialManager;
import org.chromium.base.serial.SerialPort;
import org.chromium.base.serial.SerialPortResponse;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.List;

/** Unit tests for ChromeSerialManager. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ChromeSerialManagerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SerialManager mSerialManager;

    @Mock private ChromeSerialManager.Natives mNativeMock;

    private InOrder mInOrder;

    private ChromeSerialManager mChromeSerialManager;

    private static final long NATIVE_POINTER = 123456789L;

    @Before
    public void setUp() {
        mInOrder = inOrder(mSerialManager);
        ChromeSerialManagerJni.setInstanceForTesting(mNativeMock);
        mChromeSerialManager = new ChromeSerialManager(NATIVE_POINTER, mSerialManager);
    }

    @Test
    public void registerListenerAndEnumeratePorts_readsPortsAndSendsViaJni() {
        List<SerialPort> serialPorts =
                List.of(createMockSerialPort("ttyS0"), createMockSerialPort("ttyS1"));
        when(mSerialManager.getPorts()).thenReturn(serialPorts);

        mChromeSerialManager.registerListenerAndEnumeratePorts();

        mInOrder.verify(mSerialManager).registerSerialPortListener(any(), any());
        mInOrder.verify(mSerialManager).getPorts();
        verify(mNativeMock)
                .addPortViaJni(
                        eq(NATIVE_POINTER),
                        eq("ttyS0"),
                        eq(-1),
                        eq(-1),
                        /* initialEnumeration= */ eq(true));
        verify(mNativeMock)
                .addPortViaJni(
                        eq(NATIVE_POINTER),
                        eq("ttyS1"),
                        eq(-1),
                        eq(-1),
                        /* initialEnumeration= */ eq(true));
    }

    @Test
    public void onSerialPortConnected_addPortViaJni() {
        SerialPort port = createMockSerialPort("ttyS0");

        mChromeSerialManager.onSerialPortConnected(port);

        verify(mNativeMock)
                .addPortViaJni(
                        eq(NATIVE_POINTER),
                        eq("ttyS0"),
                        eq(-1),
                        eq(-1),
                        /* initialEnumeration= */ eq(false));
    }

    @Test
    public void onSerialPortDisconnected_removePortViaJni() {
        SerialPort port = createMockSerialPort("ttyS0");

        mChromeSerialManager.onSerialPortDisconnected(port);

        verify(mNativeMock).removePortViaJni(eq(NATIVE_POINTER), eq("ttyS0"));
    }

    @Test
    public void close_unregistersListener() {
        mChromeSerialManager.registerListenerAndEnumeratePorts();

        mChromeSerialManager.close();

        verify(mSerialManager).unregisterSerialPortListener(any());
    }

    @Test
    public void openPorts_portNotFound() {
        mChromeSerialManager.registerListenerAndEnumeratePorts();

        String error = mChromeSerialManager.openPort("ttyS0");

        assertFalse(error.isEmpty());
    }

    @Test
    public void openPorts_requestsOpen() {
        SerialPort port = createMockSerialPort("ttyS0");
        when(mSerialManager.getPorts()).thenReturn(List.of(port));
        mChromeSerialManager.registerListenerAndEnumeratePorts();
        ArgumentCaptor<Integer> captor = ArgumentCaptor.forClass(Integer.class);

        String error = mChromeSerialManager.openPort("ttyS0");

        verify(port).requestOpen(captor.capture(), /* exclusive= */ eq(true), any(), any());

        assertTrue(error.isEmpty());
        int flags = captor.getValue();
        assertTrue((flags & SerialPort.OPEN_FLAG_READ_WRITE) != 0);
        assertTrue((flags & SerialPort.OPEN_FLAG_NONBLOCK) != 0);
    }

    @Test
    public void openPorts_onSuccess_returnsFileDescriptor() {
        SerialPort port = createMockSerialPort("ttyS0");
        when(mSerialManager.getPorts()).thenReturn(List.of(port));
        mChromeSerialManager.registerListenerAndEnumeratePorts();
        ArgumentCaptor<OutcomeReceiver<SerialPortResponse, Exception>> captor =
                ArgumentCaptor.forClass(OutcomeReceiver.class);

        mChromeSerialManager.openPort("ttyS0");
        verify(port).requestOpen(anyInt(), /* exclusive= */ eq(true), any(), captor.capture());
        OutcomeReceiver<SerialPortResponse, Exception> receiver = captor.getValue();
        receiver.onResult(createSerialPortResponse(port, 1234));

        verify(mNativeMock).openPathCallbackViaJni(eq(NATIVE_POINTER), eq("ttyS0"), eq(1234));
    }

    @Test
    public void openPorts_onError_returnsError() {
        SerialPort port = createMockSerialPort("ttyS0");
        when(mSerialManager.getPorts()).thenReturn(List.of(port));
        mChromeSerialManager.registerListenerAndEnumeratePorts();
        ArgumentCaptor<OutcomeReceiver<SerialPortResponse, Exception>> captor =
                ArgumentCaptor.forClass(OutcomeReceiver.class);

        mChromeSerialManager.openPort("ttyS0");
        verify(port).requestOpen(anyInt(), /* exclusive= */ eq(true), any(), captor.capture());
        OutcomeReceiver<SerialPortResponse, Exception> receiver = captor.getValue();
        receiver.onError(new Exception("test"));

        verify(mNativeMock).errorCallbackViaJni(eq(NATIVE_POINTER), eq("ttyS0"), any(), any());
    }

    private SerialPort createMockSerialPort(String name) {
        return createMockSerialPort(name, -1, -1);
    }

    private SerialPort createMockSerialPort(String name, int vendorId, int productId) {
        SerialPort mockSerialPort = mock(SerialPort.class);
        when(mockSerialPort.getName()).thenReturn(name);
        when(mockSerialPort.getVendorId()).thenReturn(vendorId);
        when(mockSerialPort.getProductId()).thenReturn(productId);
        return mockSerialPort;
    }

    private SerialPortResponse createSerialPortResponse(SerialPort port, int fd) {
        return new SerialPortResponse() {
            @Override
            public SerialPort getPort() {
                return port;
            }

            @Override
            public ParcelFileDescriptor getFileDescriptor() {
                ParcelFileDescriptor pfd = mock(ParcelFileDescriptor.class);
                when(pfd.getFd()).thenReturn(fd);
                when(pfd.detachFd()).thenReturn(fd);
                return pfd;
            }
        };
    }
}
