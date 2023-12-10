// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.services.service_manager;

import org.chromium.mojo.bindings.Interface;
import org.chromium.mojo.system.MessagePipeHandle;
import org.chromium.mojo.system.MojoException;
import org.chromium.service_manager.mojom.InterfaceProvider;

import java.util.HashMap;
import java.util.Map;

/**
 * A registry where interfaces may be registered to be exposed to another application.
 *
 * To use, define a class that implements your specific interface. Then
 * implement an InterfaceFactory that creates instances of your implementation
 * and register that on the registry with a Manager for the interface like this:
 *
 *   registry.addInterface(InterfaceType.MANAGER, factory);
 */
public class InterfaceRegistry implements InterfaceProvider {
    private final Map<String, InterfaceBinder> mBinders = new HashMap<String, InterfaceBinder>();

    public <I extends Interface> void addInterface(
            Interface.Manager<I, ? extends Interface.Proxy> manager, InterfaceFactory<I> factory) {
        mBinders.put(manager.getName(), new InterfaceBinder<I>(manager, factory));
    }

    public static InterfaceRegistry create(MessagePipeHandle pipe) {
        InterfaceRegistry registry = new InterfaceRegistry();
        InterfaceProvider.MANAGER.bind(registry, pipe);
        return registry;
    }

    @Override
    public void getInterface(String name, MessagePipeHandle pipe) {
        InterfaceBinder binder = mBinders.get(name);
        if (binder == null) {
            return;
        }
        binder.bindToMessagePipe(pipe);
    }

    @Override
    public void close() {
        mBinders.clear();
    }

    @Override
    public void onConnectionError(MojoException e) {
        close();
    }

    InterfaceRegistry() {}

    private static class InterfaceBinder<I extends Interface> {
        private Interface.Manager<I, ? extends Interface.Proxy> mManager;
        private InterfaceFactory<I> mFactory;

        public InterfaceBinder(
                Interface.Manager<I, ? extends Interface.Proxy> manager,
                InterfaceFactory<I> factory) {
            mManager = manager;
            mFactory = factory;
        }

        public void bindToMessagePipe(MessagePipeHandle pipe) {
            I impl = mFactory.createImpl();
            if (impl == null) {
                pipe.close();
                return;
            }
            mManager.bind(impl, pipe);
        }
    }
}
