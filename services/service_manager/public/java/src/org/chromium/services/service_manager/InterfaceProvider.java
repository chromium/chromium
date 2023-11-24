// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.services.service_manager;

import org.chromium.mojo.bindings.ConnectionErrorHandler;
import org.chromium.mojo.bindings.Interface;
import org.chromium.mojo.bindings.InterfaceRequest;
import org.chromium.mojo.system.Core;
import org.chromium.mojo.system.MessagePipeHandle;
import org.chromium.mojo.system.MojoException;
import org.chromium.mojo.system.Pair;

/** Provides access to interfaces exposed by an InterfaceProvider mojo interface. */
public class InterfaceProvider implements ConnectionErrorHandler {
    private Core mCore;
    private org.chromium.service_manager.mojom.InterfaceProvider.Proxy mInterfaceProvider;

    public InterfaceProvider(MessagePipeHandle pipe) {
        mCore = pipe.getCore();
        mInterfaceProvider =
                org.chromium.service_manager.mojom.InterfaceProvider.MANAGER.attachProxy(pipe, 0);
        mInterfaceProvider.getProxyHandler().setErrorHandler(this);
    }

    /**
     * Binds |request| to an implementation of I in the remote application.
     *
     * @param manager The Manager for interface I.
     * @param request The request for the interface I.
     */
    public <I extends Interface> void getInterface(
            Interface.Manager<I, ? extends Interface.Proxy> manager, InterfaceRequest<I> request) {
        mInterfaceProvider.getInterface(manager.getName(), request.passHandle());
    }

    /**
     * Binds and returns a proxy to an implementation of I in the remote application.
     *
     * @param manager The Manager for interface I.
     * @return A bound Proxy for interface I.
     */
    public <I extends Interface, P extends Interface.Proxy> P getInterface(
            Interface.Manager<I, P> manager) {
        Pair<P, InterfaceRequest<I>> result = manager.getInterfaceRequest(mCore);
        getInterface(manager, result.second);
        return result.first;
    }

    @Override
    public void onConnectionError(MojoException e) {
        mInterfaceProvider.close();
    }
}
