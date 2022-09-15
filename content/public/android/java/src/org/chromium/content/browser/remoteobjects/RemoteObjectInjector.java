// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.remoteobjects;

import org.chromium.blink.mojom.RemoteObjectGateway;
import org.chromium.blink.mojom.RemoteObjectGatewayFactory;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content_public.browser.GlobalRenderFrameHostId;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.mojo.bindings.InterfaceRequest;
import org.chromium.mojo.system.Pair;
import org.chromium.mojo.system.impl.CoreImpl;

import java.lang.annotation.Annotation;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
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
    // TODO(1191511): This is essentially implementing DocumentUserData. Once a java
    // equivalent of that is created, we should use it instead of managing RFH associated state
    // here.
    private final Map<GlobalRenderFrameHostId, RemoteObjectGatewayHelper>
            mRemoteObjectGatewayHelpers = new HashMap<>();
    private boolean mAllowInspection = true;

    public RemoteObjectInjector(WebContents webContents) {
        super(webContents);
    }

    @Override
    public void renderFrameCreated(GlobalRenderFrameHostId id) {
        if (mInjectedObjects.isEmpty()) return;

        WebContents webContents = mWebContents.get();
        if (webContents == null) return;

        RenderFrameHost frameHost = webContents.getRenderFrameHostFromId(id);
        if (frameHost == null) return;

        for (Map.Entry<String, Pair<Object, Class<? extends Annotation>>> entry :
                mInjectedObjects.entrySet()) {
            addInterfaceForFrame(
                    frameHost, entry.getKey(), entry.getValue().first, entry.getValue().second);
        }
    }

    @Override
    public void renderFrameDeleted(GlobalRenderFrameHostId id) {
        mRemoteObjectGatewayHelpers.remove(id);
    }

    public void addInterface(
            Object object, String name, Class<? extends Annotation> requiredAnnotation) {
        WebContentsImpl webContents = (WebContentsImpl) mWebContents.get();
        if (webContents == null) return;

        Pair<Object, Class<? extends Annotation>> value = mInjectedObjects.get(name);

        // Nothing to do if the named object already exists.
        if (value != null && value.first == object) return;

        if (value != null) {
            // Remove existing name for replacement.
            removeInterface(name);
        }

        mInjectedObjects.put(name, new Pair<>(object, requiredAnnotation));

        List<RenderFrameHost> frames = webContents.getAllRenderFrameHosts();
        for (RenderFrameHost frame : frames) {
            // If there's no renderer frame yet, we will add the interface when
            // it is created.
            if (frame.isRenderFrameLive()) {
                addInterfaceForFrame(frame, name, object, requiredAnnotation);
            }
        }
    }

    public void removeInterface(String name) {
        WebContentsImpl webContents = (WebContentsImpl) mWebContents.get();
        if (webContents == null) return;

        Pair<Object, Class<? extends Annotation>> value = mInjectedObjects.remove(name);
        if (value == null) return;

        List<RenderFrameHost> frames = webContents.getAllRenderFrameHosts();
        for (RenderFrameHost frame : frames) {
            removeInterfaceForFrame(frame, name, value.first);
        }
    }

    public void setAllowInspection(boolean allow) {
        WebContentsImpl webContents = (WebContentsImpl) mWebContents.get();
        if (webContents == null) return;

        mAllowInspection = allow;

        List<RenderFrameHost> frames = webContents.getAllRenderFrameHosts();
        for (RenderFrameHost frame : frames) {
            setAllowInspectionForFrame(frame);
        }
    }

    private void addInterfaceForFrame(RenderFrameHost frameHost, String name, Object object,
            Class<? extends Annotation> requiredAnnotation) {
        RemoteObjectGatewayHelper helper = getRemoteObjectGatewayHelperForFrame(frameHost);
        helper.gateway.addNamedObject(
                name, helper.registry.getObjectId(object, requiredAnnotation));
    }

    private void removeInterfaceForFrame(RenderFrameHost frameHost, String name, Object object) {
        RemoteObjectGatewayHelper helper =
                mRemoteObjectGatewayHelpers.get(frameHost.getGlobalRenderFrameHostId());
        if (helper == null) return;

        helper.gateway.removeNamedObject(name);
        helper.registry.unrefObjectByObject(object);
    }

    private void setAllowInspectionForFrame(RenderFrameHost frameHost) {
        RemoteObjectGatewayHelper helper =
                mRemoteObjectGatewayHelpers.get(frameHost.getGlobalRenderFrameHostId());
        if (helper == null) return;

        helper.host.setAllowInspection(mAllowInspection);
    }

    private RemoteObjectGatewayHelper getRemoteObjectGatewayHelperForFrame(
            RenderFrameHost frameHost) {
        GlobalRenderFrameHostId frameHostId = frameHost.getGlobalRenderFrameHostId();
        // Only create one instance of RemoteObjectHostImpl per frame and store it in a map so it is
        // reused in future calls.
        if (!mRemoteObjectGatewayHelpers.containsKey(frameHostId)) {
            RemoteObjectRegistry registry = new RemoteObjectRegistry(mRetainingSet);

            // Construct a RemoteObjectHost implementation.
            RemoteObjectHostImpl host = new RemoteObjectHostImpl(
                    new RemoteObjectAuditorImpl(), registry, mAllowInspection);

            RemoteObjectGatewayFactory factory =
                    frameHost.getInterfaceToRendererFrame(RemoteObjectGatewayFactory.MANAGER);

            Pair<RemoteObjectGateway.Proxy, InterfaceRequest<RemoteObjectGateway>> result =
                    RemoteObjectGateway.MANAGER.getInterfaceRequest(CoreImpl.getInstance());
            factory.createRemoteObjectGateway(host, result.second);
            mRemoteObjectGatewayHelpers.put(
                    frameHostId, new RemoteObjectGatewayHelper(result.first, host, registry));
        }

        return mRemoteObjectGatewayHelpers.get(frameHostId);
    }
}
