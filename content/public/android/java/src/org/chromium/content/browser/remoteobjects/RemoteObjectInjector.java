// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.remoteobjects;

import org.chromium.blink.mojom.RemoteObjectGateway;
import org.chromium.blink.mojom.RemoteObjectGatewayFactory;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.mojo.bindings.InterfaceRequest;
import org.chromium.mojo.system.Pair;
import org.chromium.mojo.system.impl.CoreImpl;

import java.lang.annotation.Annotation;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * Owned and constructed by JavascriptInjectorImpl.
 * Could possibly be flattened into JavascriptInjectorImpl eventually, but may be nice to keep
 * separate while there are separate codepaths.
 */
public final class RemoteObjectInjector extends WebContentsObserver {
    /**
     * Helper class for holding objects used to interact
     * with the RemoteObjectGateway in the renderer.
     */
    private static class RemoteObjectGatewayHelper {
        public RemoteObjectGateway.Proxy gateway;
        public RemoteObjectHostImpl host;
        public RemoteObjectRegistry registry;

        public RemoteObjectGatewayHelper(RemoteObjectGateway.Proxy newGateway,
                RemoteObjectHostImpl newHost, RemoteObjectRegistry newRegistry) {
            gateway = newGateway;
            host = newHost;
            registry = newRegistry;
        }
    }

    private final Set<Object> mRetainingSet = new HashSet<>();
    private final Map<String, Pair<Object, Class<? extends Annotation>>> mInjectedObjects =
            new HashMap<>();
    private final Map<RenderFrameHost, RemoteObjectGatewayHelper> mRemoteObjectGatewayHelpers =
            new HashMap<>();
    private boolean mAllowInspection = true;

    public RemoteObjectInjector(WebContents webContents) {
        super(webContents);
    }

    @Override
    public void renderFrameCreated(int renderProcessId, int renderFrameId) {
        if (mInjectedObjects.isEmpty()) return;

        WebContents webContents = mWebContents.get();
        if (webContents == null) return;

        RenderFrameHost frameHost =
                webContents.getRenderFrameHostFromId(renderProcessId, renderFrameId);
        if (frameHost == null) return;

        for (Map.Entry<String, Pair<Object, Class<? extends Annotation>>> entry :
                mInjectedObjects.entrySet()) {
            addInterfaceForFrame(
                    frameHost, entry.getKey(), entry.getValue().first, entry.getValue().second);
        }
    }

    public void addInterface(Object object, String name) {
        WebContents webContents = mWebContents.get();
        if (webContents == null) return;

        Pair<Object, Class<? extends Annotation>> value = mInjectedObjects.get(name);

        // Nothing to do if the named object already exists.
        if (value != null && value.first == object) return;

        if (value != null) {
            // Remove existing name for replacement.
            removeInterface(name);
        }

        // TODO(crbug.com/1105935): find a way to make requiredAnnotation available to
        // mRemoteObjectHost when JavascriptInjectorImpl calls addInterface().
        Class<? extends Annotation> requiredAnnotation = null;
        mInjectedObjects.put(name, new Pair<>(object, requiredAnnotation));

        // TODO(crbug.com/1105935): the objects need to be injected into all frames, not just the
        // main one.
        addInterfaceForFrame(webContents.getMainFrame(), name, object, requiredAnnotation);
    }

    public void removeInterface(String name) {
        WebContents webContents = mWebContents.get();
        if (webContents == null) return;

        Pair<Object, Class<? extends Annotation>> value = mInjectedObjects.remove(name);
        if (value == null) return;

        // TODO(crbug.com/1105935): the objects need to be removed from all frames, not just the
        // main one.
        removeInterfaceForFrame(webContents.getMainFrame(), name, value.first);
    }

    public void setAllowInspection(boolean allow) {
        mAllowInspection = allow;
    }

    private void addInterfaceForFrame(RenderFrameHost frameHost, String name, Object object,
            Class<? extends Annotation> requiredAnnotation) {
        RemoteObjectGatewayHelper helper =
                getRemoteObjectGatewayHelperForFrame(frameHost, requiredAnnotation);
        helper.gateway.addNamedObject(name, helper.registry.getObjectId(object));
    }

    private void removeInterfaceForFrame(RenderFrameHost frameHost, String name, Object object) {
        RemoteObjectGatewayHelper helper = mRemoteObjectGatewayHelpers.get(frameHost);
        if (helper == null) return;

        helper.gateway.removeNamedObject(name);
        helper.registry.unrefObjectByObject(object);
    }

    private RemoteObjectGatewayHelper getRemoteObjectGatewayHelperForFrame(
            RenderFrameHost frameHost, Class<? extends Annotation> requiredAnnotation) {
        // Only create one instance of RemoteObjectHostImpl per frame and store it in a map so it is
        // reused in future calls.
        if (!mRemoteObjectGatewayHelpers.containsKey(frameHost)) {
            RemoteObjectRegistry registry = new RemoteObjectRegistry(mRetainingSet);

            // Construct a RemoteObjectHost implementation.
            RemoteObjectHostImpl host = new RemoteObjectHostImpl(
                    requiredAnnotation, new RemoteObjectAuditorImpl(), registry, mAllowInspection);

            RemoteObjectGatewayFactory factory = frameHost.getRemoteInterfaces().getInterface(
                    RemoteObjectGatewayFactory.MANAGER);

            Pair<RemoteObjectGateway.Proxy, InterfaceRequest<RemoteObjectGateway>> result =
                    RemoteObjectGateway.MANAGER.getInterfaceRequest(CoreImpl.getInstance());
            factory.createRemoteObjectGateway(host, result.second);
            mRemoteObjectGatewayHelpers.put(
                    frameHost, new RemoteObjectGatewayHelper(result.first, host, registry));
        }

        return mRemoteObjectGatewayHelpers.get(frameHost);
    }
}
