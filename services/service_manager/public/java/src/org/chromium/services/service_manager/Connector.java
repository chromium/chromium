// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.services.service_manager;

import org.chromium.mojo.bindings.ConnectionErrorHandler;
import org.chromium.mojo.bindings.Interface;
import org.chromium.mojo.bindings.InterfaceRequest;
import org.chromium.mojo.system.MessagePipeHandle;
import org.chromium.mojo.system.MojoException;
import org.chromium.service_manager.mojom.Identity;
import org.chromium.service_manager.mojom.ServiceFilter;

/** This class exposes the ability to bind interfaces from other services in the system. */
public class Connector implements ConnectionErrorHandler {
    private org.chromium.service_manager.mojom.Connector.Proxy mConnector;

    private static class ConnectorBindInterfaceResponseImpl
            implements org.chromium.service_manager.mojom.Connector.BindInterface_Response {
        @Override
        public void call(int result, Identity identity) {}
    }

    public Connector(MessagePipeHandle handle) {
        mConnector = org.chromium.service_manager.mojom.Connector.MANAGER.attachProxy(handle, 0);
        mConnector.getProxyHandler().setErrorHandler(this);
    }

    /**
     * Asks a service to bind an interface request.
     *
     * @param serviceName The name of the service.
     * @param interfaceName The name of interface I.
     * @param request The request for the interface I.
     */
    public <I extends Interface, P extends Interface.Proxy> void bindInterface(
            String serviceName, String interfaceName, InterfaceRequest<I> request) {
        ServiceFilter filter = new ServiceFilter();
        filter.serviceName = serviceName;

        org.chromium.service_manager.mojom.Connector.BindInterface_Response callback =
                new ConnectorBindInterfaceResponseImpl();
        mConnector.bindInterface(
                filter,
                interfaceName,
                request.passHandle(),
                org.chromium.service_manager.mojom.BindInterfacePriority.IMPORTANT,
                callback);
    }

    @Override
    public void onConnectionError(MojoException e) {
        mConnector.close();
    }
}
